/*
  Separate sex model with sex-specific mutation rates 
  and fitness effects of mutations being different in each sex.

  \include sex_limited.cc
  Total madness:
  1. Different mutation rates to variants affecting traits in each sex
  2. Mutation effects are sex-limited ("male" mutations only affect trait values in males, etc.)
  3. House of cards model of additive fitness effects and then stabilizing selection on trait according to a unit Gaussian
*/

#include <limits>
#include <algorithm>

#include <boost/container/list.hpp>
#include <boost/container/vector.hpp>
#include <boost/pool/pool_alloc.hpp>
#include <boost/unordered_set.hpp>
#include <boost/functional/hash.hpp>

//Main fwdpp library header
#include <fwdpp/diploid.hh>
//Include the necessary "sugar" components
//We need to get the 'deep' version of singlepop, as we need to make a custom singlepop_serialized_t for our sim
#define FWDPP_SUGAR_USE_BOOST
#include <fwdpp/sugar/singlepop.hpp>
#include <fwdpp/sugar/GSLrng_t.hpp>
#include <fwdpp/sugar/serialization.hpp>
#include <fwdpp/sugar/infsites.hpp>
#include <fwdpp/experimental/sample_diploid.hpp>
//FWDPP-related stuff

struct sex_specific_mutation : public KTfwd::mutation_base
{
  //Effect size
  double s;
  //The effect size applies to this sex, is 0 otherwise
  bool sex;
  sex_specific_mutation(const double & __pos, const double & __s, const bool & __sex,
			const unsigned & __n, const bool & __neutral)
    : KTfwd::mutation_base(__pos,__n,__neutral),s(__s),sex(__sex)
  {	
  }
};

using mtype = sex_specific_mutation;
using mlist_t = boost::container::list<mtype,boost::fast_pool_allocator<mtype> >;
using gamete_t = KTfwd::gamete_base<mtype,mlist_t>;
using glist_t = boost::container::list<gamete_t, boost::fast_pool_allocator<gamete_t>>;

//We need to define a custom diploid genotype for our model
struct diploid_t : public KTfwd::tags::custom_diploid_t
{
  using first_type = glist_t::iterator;
  using second_type = glist_t::iterator;
  first_type first;
  second_type second;
  bool sex; //male = false, I guess...
  //constructors, etc.
  diploid_t() : first(first_type()),second(second_type()),sex(false) {}
  //"perfect forwarding" constructor does not work with iterator from boost containers...
  //diploid_t(first_type && g1, first_type && g2) : first(std::forward(g1)),second(std::forward(g2)),i(numeric_limits<unsigned>::max()) {}
  diploid_t(first_type g1, first_type g2) : first(g1),second(g2),sex(false) {}
  //The following constructors SHOULD be generated automagically by your compiler, so you don't have to:
  //(no idea what, if any, performance effect this may have.  Worst case is prob. the move constructor doesn't get auto-generated...
  //diploid_t( const diploid_t & ) = default;
  //diploid_t( diploid_t && ) = default;
  //diploid_t & operator=(const diploid_t &) = default;
};

/*
  Define our our population type via sugar template
  In 0.3.1, I introduced the ability to use custom diploid types with the library's sugar layer.
*/
using poptype = KTfwd::singlepop_serialized<mtype,KTfwd::mutation_writer,KTfwd::mutation_reader<mtype>,diploid_t>;


/*
  We will use a gsl_rng_mt19937 as our RNG.
  This type is implicitly convertible to gsl_rng *,
  and auto-handles the gsl_rng_free steps, etc.
*/
using GSLrng = KTfwd::GSLrng_t<KTfwd::GSL_RNG_MT19937>;

mtype sex_specific_mut_model( gsl_rng * r,
			      poptype::mlist_t * mutations,
			      poptype::lookup_table_t * lookup,
			      const double & mu_total,
			      const double & mu_male,
			      const double & mu_female,
			      const double & sigma )
{
  double pos = gsl_rng_uniform(r);
  while(lookup->find(pos) != lookup->end())
    {
      pos = gsl_rng_uniform(r);
    }
  lookup->insert(pos);
  double u = gsl_rng_uniform(r);
  if(u <= mu_male/mu_total)
    {
      return mtype(pos,gsl_ran_gaussian(r,sigma),false,1,false);
    }
  else if (u <= (mu_male+mu_female)/mu_total)
    {
      return mtype(pos,gsl_ran_gaussian(r,sigma),true,1,false);
    }
  //Otherwise, neutral mutation
  //We "hack" this and assign the mutation a "male" type,
  //As they'll never be used in a fitness calc,
  //as they'll be stored in mutations rather than
  //smutations
  return mtype(pos,0.,false,1,true);
}

//Now, we need our own "rules"
struct sexSpecificRules
{
  mutable double wbar,mwbar,fwbar;
  mutable std::vector<double> male_fitnesses,female_fitnesses;
  mutable std::vector<size_t> male_indexes,female_indexes;
  mutable KTfwd::fwdpp_internal::gsl_ran_discrete_t_ptr male_lookup,female_lookup;
  //! \brief Constructor
  sexSpecificRules() : wbar(0.),
		       male_fitnesses(std::vector<double>()),
		       female_fitnesses(std::vector<double>()),
		       male_indexes(std::vector<size_t>()),
		       female_indexes(std::vector<size_t>()),
		       male_lookup(KTfwd::fwdpp_internal::gsl_ran_discrete_t_ptr(nullptr)),
		       female_lookup(KTfwd::fwdpp_internal::gsl_ran_discrete_t_ptr(nullptr))
  {
  }
  //! \brief The "fitness manager"
  template<typename T,typename fitness_func>
  void w(const T * diploids,
	 const fitness_func & ff)const
  {
    using diploid_geno_t = typename T::value_type;
    unsigned N_curr = diploids->size();
    if(male_fitnesses.size() < N_curr) {
      male_fitnesses.resize(N_curr);
      male_indexes.resize(N_curr);
    }
    if(female_fitnesses.size() < N_curr) {
      female_fitnesses.resize(N_curr);
      female_indexes.resize(N_curr);
    }
    wbar = mwbar = fwbar = 0.;
    
    auto dptr = diploids->begin();

    double w;    
    unsigned male=0,female=0;
    for( unsigned i = 0 ; i < N_curr ; ++i )
      {
	(dptr+i)->first->n = 0;
	(dptr+i)->second->n = 0;
	w = KTfwd::fwdpp_internal::diploid_fitness_dispatch(ff,(dptr+i),
							    typename KTfwd::traits::is_custom_diploid_t<diploid_geno_t>::type());
	  if(!(dptr+i)->sex) {
	    male_fitnesses[male]=w;
	    mwbar += w;
	    male_indexes[male++]=i;
	  } else {
	    female_fitnesses[female]=w;
	    fwbar += w;
	    female_indexes[female++]=i;
	  }
	wbar += w;
      }
    wbar /= double(diploids->size());
    mwbar /= double(male);
    fwbar /= double(female);

    /*
      Biological point:
      
      We want to ensure that ANY individual is chosen as a parent proportional to w/wbar (conditional on the
      individual being of the desired "sex".

      Thus, we need to transform all male and female fitnesses by wbar.

      If we did not do this transform, individuals would be chosen according to w/wbar_sex, which is desired in 
      some cases, but not here...
     */
    std::transform( male_fitnesses.begin(),male_fitnesses.begin()+male, 
		    male_fitnesses.begin(),std::bind(std::divides<double>(),std::placeholders::_1,wbar) );
    std::transform( female_fitnesses.begin(),female_fitnesses.begin()+female, 
		    female_fitnesses.begin(),std::bind(std::divides<double>(),std::placeholders::_1,wbar) );
    /*!
      Black magic alert:
      fwdpp_internal::gsl_ran_discrete_t_ptr contains a std::unique_ptr wrapping the GSL pointer.
      This type has its own deleter, which is convenient, because
      operator= for unique_ptrs automagically calls the deleter before assignment!
      Details: http://www.cplusplus.com/reference/memory/unique_ptr/operator=
    */
    male_lookup = KTfwd::fwdpp_internal::gsl_ran_discrete_t_ptr(gsl_ran_discrete_preproc(male,&male_fitnesses[0]));
    female_lookup = KTfwd::fwdpp_internal::gsl_ran_discrete_t_ptr(gsl_ran_discrete_preproc(female,&female_fitnesses[0]));
  }

  //! \brief Pick parent one
  inline size_t pick1(gsl_rng * r) const
  {
    return male_indexes[gsl_ran_discrete(r,male_lookup.get())];
  }

  //! \brief Pick parent 2.  Parent 1's data are passed along for models where that is relevant
  template<typename diploid_itr_t>
  inline size_t pick2(gsl_rng * r, const size_t & , const diploid_itr_t & , const double & ) const
  {
    return female_indexes[gsl_ran_discrete(r,female_lookup.get())];
  }
  
  //! \brief Update some property of the offspring based on properties of the parents
  template<typename offspring_itr_t, typename parent_itr_t>
  void update(gsl_rng * r,offspring_itr_t offspring, parent_itr_t p1_itr, parent_itr_t p2_itr ) const
  {
    offspring->sex = (gsl_rng_uniform(r) <= 0.5);
    static_assert(!std::is_const<decltype(offspring)>::value, "offspring_itr_t must not be const");
    static_assert(!std::is_const<decltype(*p1_itr)>::value, "parent_itr_t must not be const");
    static_assert(!std::is_const<decltype(*p2_itr)>::value, "parent_itr_t must not be const");
    return;
  }  
};

//We need a fitness model
double sex_specific_fitness( const poptype::dipvector_t::const_iterator & dip, gsl_rng * r, const double & sigmaE )
{
  double trait_value = std::accumulate( dip->first->smutations.begin(),
					dip->first->smutations.end(),
					0.,[&dip](const double & a, const poptype::mlist_t::const_iterator & m)
					{
					  return a + ((dip->sex==m->sex) ? m->s : 0.);
					} );
  trait_value += std::accumulate( dip->second->smutations.begin(),
				  dip->second->smutations.end(),
				  0.,[&dip](const double & a, const poptype::mlist_t::const_iterator & m)
				  {
				    return a + ((dip->sex==m->sex) ? m->s : 0.);
				  } );
  return std::exp( -std::pow(trait_value+gsl_ran_gaussian(r,sigmaE),2.)/2.);
}

int main(int argc, char ** argv)
{
  if (argc != 12)
    {
      std::cerr << "Too few arguments.\n"
		<< "Usage: " << argv[0]
		<< " N mu_neutral mu_male mu_female sigma_mu sigma_e recrate ngens samplesize nreps seed\n";
      exit(10);
    }
  int argument=1;
  
  const unsigned N = atoi(argv[argument++]);
  const double mu_neutral = atof(argv[argument++]);
  const double mu_male = atof(argv[argument++]);
  const double mu_female = atof(argv[argument++]);
  const double sigma = atof(argv[argument++]);
  const double sigmaE = atof(argv[argument++]);
  const double recrate = atof(argv[argument++]);
  const unsigned ngens = atoi(argv[argument++]);
  const unsigned samplesize1 = atoi(argv[argument++]);
  const unsigned nreps = atoi(argv[argument++]);
  const unsigned seed = atoi(argv[argument++]);

  GSLrng rng(seed);
  std::function<double(void)> recmap = std::bind(gsl_rng_uniform,rng.get()); //uniform crossover map
  const double mu_total = mu_neutral+mu_male+mu_female;
  sexSpecificRules rules;
  for( unsigned rep = 0 ; rep < nreps ; ++rep )
    {
      poptype pop(N);
      //Assign "sex"
      for( auto dip = pop.diploids.begin() ; dip != pop.diploids.end() ; ++dip )
	{
	  dip->sex = (gsl_rng_uniform(rng.get()) <= 0.5); //false = male, true = female.
	}
      for( unsigned generation = 0 ; generation < ngens ; ++generation )
	{
	  //double wbar = KTfwd::sample_diploid(rng,
	  double wbar = KTfwd::experimental::sample_diploid(rng.get(),
							    &pop.gametes,
							    &pop.diploids,
							    &pop.mutations,
							    N,
							    mu_total,
							    std::bind(sex_specific_mut_model,rng.get(),std::placeholders::_1,
								      &pop.mut_lookup,mu_total,mu_male,mu_female,sigma),
							    std::bind(KTfwd::genetics101(),std::placeholders::_1,std::placeholders::_2,std::placeholders::_3,
								      std::ref(pop.neutral),std::ref(pop.selected),
								      &pop.gametes,
								      recrate, 
								      rng.get(),
								      recmap),
							    std::bind(KTfwd::insert_at_end<poptype::mutation_t,poptype::mlist_t>,std::placeholders::_1,std::placeholders::_2),
							    std::bind(KTfwd::insert_at_end<poptype::gamete_t,poptype::glist_t>,std::placeholders::_1,std::placeholders::_2),
							    std::bind(sex_specific_fitness,std::placeholders::_1,rng.get(),sigmaE),
							    std::bind(KTfwd::mutation_remover(),std::placeholders::_1,0,2*pop.N),
							    0., //Gotta pass the "selfing" rate, even though it makes no sense for this model.  API tradeoff for flexibility...
							    rules
							    );
	  KTfwd::remove_fixed_lost(&pop.mutations,&pop.fixations,&pop.fixation_times,&pop.mut_lookup,generation,2*pop.N);
	}
      Sequence::SimData neutral_muts,selected_muts;
      
      //Take a sample of size samplesize1.  Two data blocks are returned, one for neutral mutations, and one for selected
      std::pair< std::vector< std::pair<double,std::string> >,
		 std::vector< std::pair<double,std::string> > > sample = KTfwd::ms_sample_separate(rng.get(),&pop.diploids,samplesize1);
      
      neutral_muts.assign( sample.first.begin(), sample.first.end() );
      selected_muts.assign( sample.second.begin(), sample.second.end() );

      std::cout << neutral_muts << '\n' << selected_muts << '\n';
    }
}
