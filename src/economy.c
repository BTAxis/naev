/*
 * See Licensing and Copyright notice in naev.h
 */

/**
 * @file economy.c
 *
 * @brief Handles economy stuff.
 *
 * Economy is handled with Nodal Analysis.  Systems are modelled as nodes,
 *  jump routes are resistances and production is modelled as node intensity.
 *  This is then solved with linear algebra after each time increment.
 */


#include "economy.h"

#include "naev.h"

#include <stdio.h>
#include "nstring.h"
#include <stdint.h>

#ifdef HAVE_SUITESPARSE_CS_H
#include <suitesparse/cs.h>
#else
#include <cs.h>
#endif

#include "nxml.h"
#include "ndata.h"
#include "log.h"
#include "spfx.h"
#include "pilot.h"
#include "rng.h"
#include "space.h"
#include "ntime.h"


#define XML_COMMODITY_ID      "Commodities" /**< XML document identifier */
#define XML_COMMODITY_TAG     "commodity" /**< XML commodity identifier. */


/*
 * Economy Nodal Analysis parameters.
 */
#define ECON_BASE_RES      30. /**< Base resistance value for any system. */
#define ECON_SELF_RES      3. /**< Additional resistance for the self node. */
#define ECON_FACTION_MOD   0.1 /**< Modifier on Base for faction standings. */
#define ECON_PROD_VAR      0.01 /**< Defines the variability of production. */
#define ECON_DEMAND_VAR      0.01 /**< Defines the variability of demand. */

#define ECONOMY_POWER_BASE    1.1 /**< The base value for commodity price calculations. Must be greater than 1. */

/* commodity stack */
Commodity* commodity_stack = NULL; /**< Contains all the commodities. */
int commodity_nstack       = 0; /**< Number of commodities in the stack. */

/* systems stack. */
extern StarSystem *systems_stack; /**< Star system stack. */
extern int systems_nstack; /**< Number of star systems. */

/* planets stack. */
extern Planet *planet_stack; /**< Star system stack. */
extern int planet_nstack; /**< Number of star systems. */

/*
 * Nodal analysis simulation for dynamic economies.
 */
static int econ_initialized   = 0; /**< Is economy system initialized? */
static cs *econ_G             = NULL; /**< Admittance matrix. */

static ntime_t econ_lastUpdate; /**< The last time the economy was updated. */


/*
 * Prototypes.
 */
/* Commodity. */
static void commodity_freeOne( Commodity* com );
static int commodity_parse( Commodity *temp, xmlNodePtr parent );
/* Economy. */
static double econ_calcJumpR( StarSystem *A, StarSystem *B );
static int econ_createGMatrix (void);
credits_t economy_getPrice( const Commodity *com,
      const StarSystem *sys, const Planet *p ); /* externed in land.c */


/**
 * @brief Converts credits to a usable string for displaying.
 *
 *    @param[out] str Output is stored here, must have at least a length of 32
 *                     char.
 *    @param credits Credits to display.
 *    @param decimals Decimals to use.
 */
void credits2str( char *str, credits_t credits, int decimals )
{
   if (decimals < 0)
      nsnprintf( str, ECON_CRED_STRLEN, "%"CREDITS_PRI, credits );
   else if (credits >= 1000000000000000LL)
      nsnprintf( str, ECON_CRED_STRLEN, "%.*fQ", decimals, (double)credits / 1000000000000000. );
   else if (credits >= 1000000000000LL)
      nsnprintf( str, ECON_CRED_STRLEN, "%.*fT", decimals, (double)credits / 1000000000000. );
   else if (credits >= 1000000000L)
      nsnprintf( str, ECON_CRED_STRLEN, "%.*fB", decimals, (double)credits / 1000000000. );
   else if (credits >= 1000000)
      nsnprintf( str, ECON_CRED_STRLEN, "%.*fM", decimals, (double)credits / 1000000. );
   else if (credits >= 1000)
      nsnprintf( str, ECON_CRED_STRLEN, "%.*fK", decimals, (double)credits / 1000. );
   else
      nsnprintf (str, ECON_CRED_STRLEN, "%"CREDITS_PRI, credits );
}


/**
 * @brief Gets a commodity by name.
 *
 *    @param name Name to match.
 *    @return Commodity matching name.
 */
Commodity* commodity_get( const char* name )
{
   int i;
   for (i=0; i<commodity_nstack; i++)
      if (strcmp(commodity_stack[i].name,name)==0)
         return &commodity_stack[i];

   WARN("Commodity '%s' not found in stack", name);
   return NULL;
}


/**
 * @brief Gets a commodity by name without warning.
 *
 *    @param name Name to match.
 *    @return Commodity matching name.
 */
Commodity* commodity_getW( const char* name )
{
   int i;
   for (i=0; i<commodity_nstack; i++)
      if (strcmp(commodity_stack[i].name,name)==0)
         return &commodity_stack[i];
   return NULL;
}


/**
 * @brief Frees a commodity.
 *
 *    @param com Commodity to free.
 */
static void commodity_freeOne( Commodity* com )
{
   if (com->name)
      free(com->name);
   if (com->description)
      free(com->description);

   /* Clear the memory. */
   memset(com, 0, sizeof(Commodity));
}


/**
 * @brief Function meant for use with C89, C99 algorithm qsort().
 *
 *    @param commodity1 First argument to compare.
 *    @param commodity2 Second argument to compare.
 *    @return -1 if first argument is inferior, +1 if it's superior, 0 if ties.
 */
int commodity_compareTech( const void *commodity1, const void *commodity2 )
{
   const Commodity *c1, *c2;

   /* Get commodities. */
   c1 = * (const Commodity**) commodity1;
   c2 = * (const Commodity**) commodity2;

   /* Compare price. */
   if (c1->price < c2->price)
      return +1;
   else if (c1->price > c2->price)
      return -1;

   /* It turns out they're the same. */
   return strcmp( c1->name, c2->name );
}


/**
 * @brief Throws cargo out in space graphically.
 *
 *    @param pilot ID of the pilot throwing the stuff out
 *    @param com Commodity to throw out.
 *    @param quantity Quantity thrown out.
 */
void commodity_Jettison( int pilot, Commodity* com, int quantity )
{
   (void)com;
   int i;
   Pilot* p;
   int n, effect;
   double px,py, bvx, bvy, r,a, vx,vy;

   p   = pilot_get( pilot );

   n   = MAX( 1, RNG(quantity/10, quantity/5) );
   px  = p->solid->pos.x;
   py  = p->solid->pos.y;
   bvx = p->solid->vel.x;
   bvy = p->solid->vel.y;
   for (i=0; i<n; i++) {
      effect = spfx_get("cargo");

      /* Radial distribution gives much nicer results */
      r  = RNGF()*25 - 12.5;
      a  = 2. * M_PI * RNGF();
      vx = bvx + r*cos(a);
      vy = bvy + r*sin(a);

      /* Add the cargo effect */
      spfx_add( effect, px, py, vx, vy, SPFX_LAYER_BACK );
   }
}

/**
 * @brief Loads a commodity.
 *
 *    @param temp Commodity to load data into.
 *    @param parent XML node to load from.
 *    @return Commodity loaded from parent.
 */
static int commodity_parse( Commodity *temp, xmlNodePtr parent )
{
   xmlNodePtr node;
   char buf[PATH_MAX], *dat;
   uint32_t ndat;

   /* Clear memory. */
   memset( temp, 0, sizeof(Commodity) );

   /* Get name. */
   xmlr_attr( parent, "name", temp->name );
   if (temp->name == NULL)
      WARN("Commodity from "COMMODITY_DATA_PATH" has invalid or no name");

   /* Parse body. */
   node = parent->xmlChildrenNode;
   do {
      xml_onlyNodes(node);
      xmlr_strd(node, "description", temp->description);
      xmlr_int(node, "price", temp->base_price);

      if (xml_isNode(node, "lua")) {
         if (temp->lua != NULL)
            WARN("Faction '%s' has duplicate 'lua' tag.", temp->name);
         nsnprintf( buf, sizeof(buf), "dat/commodities/%s.lua", xml_raw(node) );
         temp->lua = nlua_newState();
         nlua_loadStandard( temp->lua, 0 );
         dat = ndata_read( buf, &ndat );
         if (luaL_dobuffer(temp->lua, dat, ndat, buf) != 0) {
            WARN("Failed to run lua script: %s\n"
                  "%s\n"
                  "Most likely Lua file has improper syntax, please check",
                  buf, lua_tostring(temp->lua,-1));
            lua_close( temp->lua );
            temp->lua = NULL;
         }
         free(dat);
         continue;
      }
      else
         WARN("Commodity '%s' has unknown node '%s'.", temp->name, node->name);
   } while (xml_nextNode(node));

   return 0;
}


int commodity_load (void)
{
   int mem;
   uint32_t bufsize;
   char *buf = ndata_read( COMMODITY_DATA_PATH, &bufsize);

   xmlNodePtr parent, node;
   xmlDocPtr doc = xmlParseMemory( buf, bufsize );

   parent = doc->xmlChildrenNode; /* Factions node */
   if (!xml_isNode(parent,XML_COMMODITY_ID)) {
      ERR("Malformed "COMMODITY_DATA_PATH" file: missing root element '"XML_COMMODITY_ID"'");
      return -1;
   }

   node = parent->xmlChildrenNode; /* first faction node */
   if (node == NULL) {
      ERR("Malformed "COMMODITY_DATA_PATH" file: does not contain elements");
      return -1;
   }

   mem = 0;
   do {
      if (xml_isNode(node,XML_COMMODITY_TAG)) {
         /* See if must grow memory.  */
         commodity_nstack++;
         if (commodity_nstack > mem) {
            mem ++;
            commodity_stack = realloc(commodity_stack, sizeof(Commodity)*mem);
         }

         /* Load faction. */
         commodity_parse(&commodity_stack[commodity_nstack-1], node);
      }
   } while (xml_nextNode(node));

   /* Shrink to minimum size. */
   commodity_stack = realloc(commodity_stack, sizeof(Commodity)*commodity_nstack);

   return 0;
}


/**
 * @brief Frees all the loaded commodities.
 */
void commodity_free (void)
{
   int i,j;
   for (i=0; i<commodity_nstack; i++)
      commodity_freeOne( &commodity_stack[i] );
   free( commodity_stack );
   commodity_stack = NULL;
   commodity_nstack = 0;

   /* More clean up. */
   for (i=0; i<planet_nstack; i++) {
      for (j=0; j<commodity_nstack; j++)
         commodity_freeOne( &planet_stack[i].commodities[j] );
      free( planet_stack[i].commodities );
   }
}


/**
 * @brief Gets the price of a good on a planet in a system.
 *
 *    @param com Commodity to get price of.
 *    @param sys System to get price of commodity.
 *    @param p Planet to get price of commodity.
 *    @return The price of the commodity.
 */
credits_t economy_getPrice( const Commodity *com,
      const StarSystem *sys, const Planet *p )
{
   (void) sys;
   int i;
   double price;

   /* Find what commodity that is. */
   for (i=0; i<commodity_nstack; i++)
      if (strcmp(p->commodities[i].name, com->name) == 0)
         break;

   /* Check if found. */
   if (i >= commodity_nstack) {
      WARN("Price for commodity '%s' not known.", com->name);
      return 0;
   }

   /* Calculate price. */
   price = p->commodities[i].price;
   return (credits_t) price;
}


/**
 * @brief Calculates the resistance between two star systems.
 *
 *    @param A Star system to calculate the resistance between.
 *    @param B Star system to calculate the resistance between.
 *    @return Resistance between A and B.
 */
static double econ_calcJumpR( StarSystem *A, StarSystem *B )
{
   double R;

   /* Set to base to ensure price change. */
   R = ECON_BASE_RES;

   /* Modify based on system conditions. */
   R += (A->nebu_density + B->nebu_density) / 1000.; /* Density shouldn't affect much. */
   R += (A->nebu_volatility + B->nebu_volatility) / 100.; /* Volatility should. */

   /* Modify based on global faction. */
   if ((A->faction != -1) && (B->faction != -1)) {
      if (areEnemies(A->faction, B->faction))
         R += ECON_FACTION_MOD * ECON_BASE_RES;
      else if (areAllies(A->faction, B->faction))
         R -= ECON_FACTION_MOD * ECON_BASE_RES;
   }

   /* @todo Modify based on fleets. */

   return R;
}


/**
 * @brief Calculates the intensity in a system node.
 *
 * @todo Make it time/item dependent.
 */
static double econ_calcSysI( ntime_t dt, StarSystem *sys, int commodity )
{
   int i;
   double I;
   double prodfactor, p;
   double demandfactor, d;
   double supply, demand;
   double ddt;
   Planet *planet;
   lua_State *L;
   int errf=0;

   L = commodity_stack[commodity].lua;

   if (L == NULL)
      return 0;

//   ddt = (double)(dt / NTIME_UNIT_LENGTH);
   ddt = dt;

   /* Calculate production level. */
   p = 0.;
   d = 0.;
   for (i=0; i<sys->nplanets; i++) {
      planet = sys->planets[i];
      if (planet_hasService(planet, PLANET_SERVICE_INHABITED)) {

         lua_getglobal( L, "calc_supplyDemand" );      /* f */
//         lua_pushnumber(L,planet_stack[j].

         /* run lua supply/demand script */
         lua_pcall(L, 0, 2, errf);

         /* Get return values */
         if (lua_isnumber(L,-1) && lua_isnumber(L,-2)) { /* supply, demand */
            demand = lua_tonumber(L,-1);
            supply = lua_tonumber(L,-2);
         }
         else {
            demand = 0;
            supply = 0;
         }
         lua_pop(L,-1);
         lua_pop(L,-2);

         /* Supply and demand may not be negative. */
         supply = MAX(0, supply);
         demand = MAX(0, demand);

         /*
          * Calculate production.
          */
         /* We base off the current production. */
         prodfactor  = planet->commodities[commodity].supply;
         /* Add a variability factor based on the Gaussian distribution. */
         prodfactor += ECON_PROD_VAR * ddt;
         /* Add a tendency to return to the planet's base production. */
         prodfactor -= ECON_PROD_VAR * (supply - prodfactor)*ddt;
         /* Save for next iteration. */
         planet->commodities[commodity].supply = prodfactor;
         /* We base off the sqrt of the population otherwise it changes too fast. */
         p += prodfactor;

         /*
          * Calculate demand.
          */
         /* We base off the current demand. */
         demandfactor  = planet->commodities[commodity].demand;
         /* Add a variability factor based on the Gaussian distribution. */
         demandfactor += ECON_DEMAND_VAR * ddt;
         /* Add a tendency to return to the planet's base demand. */
         demandfactor -= ECON_DEMAND_VAR * (demand - demandfactor)*ddt;
         /* Save for next iteration. */
         planet->commodities[commodity].demand = demandfactor;
         /* We base off the sqrt of the population otherwise it changes too fast. */
         d += demandfactor;
      }
   }

   /* The intensity is basically the modified production. */
   I = pow(ECONOMY_POWER_BASE, (d - p) / (ddt + 1));

   return I;
}


/**
 * @brief Creates the admittance matrix.
 *
 *    @return 0 on success.
 */
static int econ_createGMatrix (void)
{
   int ret;
   int i, j;
   double R, Rsum;
   cs *M;
   StarSystem *sys;

   /* Create the matrix. */
   M = cs_spalloc( systems_nstack, systems_nstack, 1, 1, 1 );
   if (M == NULL)
      ERR("Unable to create CSparse Matrix.");

   /* Fill the matrix. */
   for (i=0; i < systems_nstack; i++) {
      sys   = &systems_stack[i];
      Rsum = 0.;

      /* Set some values. */
      for (j=0; j < sys->njumps; j++) {

         /* Get the resistances. */
         R     = econ_calcJumpR( sys, sys->jumps[j].target );
         R     = 1./R; /* Must be inverted. */
         Rsum += R;

         /* Matrix is symmetrical and non-diagonal is negative. */
         ret = cs_entry( M, i, sys->jumps[j].target->id, -R );
         if (ret != 1)
            WARN("Unable to enter CSparse Matrix Cell.");
         ret = cs_entry( M, sys->jumps[j].target->id, i, -R );
         if (ret != 1)
            WARN("Unable to enter CSparse Matrix Cell.");
      }

      /* Set the diagonal. */
      Rsum += 1./ECON_SELF_RES; /* We add a resistance for dampening. */
      cs_entry( M, i, i, Rsum );
   }

   /* Compress M matrix and put into G. */
   if (econ_G != NULL)
      cs_spfree( econ_G );
   econ_G = cs_compress( M );
   if (econ_G == NULL)
      ERR("Unable to create economy G Matrix.");

   /* Clean up. */
   cs_spfree(M);

   return 0;
}


/**
 * @brief Initializes the economy.
 *
 *    @return 0 on success.
 */
int economy_init (void)
{	
   int i,j;

   /* Must not be initialized. */
   if (econ_initialized)
      return 0;

   /* Allocate price space. */
   for (i=0; i<planet_nstack; i++) {
      planet_stack[i].commodities = malloc(commodity_nstack * sizeof(Commodity));

      for (j=0; j<commodity_nstack; j++)
         planet_stack[i].commodities[j] = commodity_stack[j];
   }

   /* Mark economy as initialized. */
   econ_initialized = 1;

   econ_lastUpdate = 0;

   /* Refresh economy. */
   economy_refresh();

   return 0;
}


/**
 * @brief Regenerates the economy matrix.  Should be used if the universe
 *  changes in any permanent way.
 */
int economy_refresh (void)
{
   /* Economy must be initialized. */
   if (econ_initialized == 0)
      return 0;

   /* Create the resistance matrix. */
   if (econ_createGMatrix())
      return -1;

   /* Initialize the prices. */
   economy_update();

   return 0;
}


/**
 * @brief Updates the economy.
 */
int economy_update()
{
   int ret;
   int i, j, k;
   ntime_t dt;
   double *X;
   double scale, offset;
   double min, max;

   /* Economy must be initialized. */
   if (econ_initialized == 0)
      return 0;

   dt = ntime_get() - econ_lastUpdate;
   econ_lastUpdate = ntime_get();

   /* Create the vector to solve the system. */
   X = malloc(sizeof(double)*systems_nstack);
   if (X == NULL) {
      WARN("Out of Memory!");
      return -1;
   }

   /* Calculate the results for each price set. */
   for (j=0; j<commodity_nstack; j++) {

      if (commodity_stack[j].lua == NULL)
         continue;

      /* First we must load the vector with intensities. */
      for (i=0; i<systems_nstack; i++)
         X[i] = econ_calcSysI( dt, &systems_stack[i], j );

      /* Solve the system. */
      /** @TODO This should be improved to try to use better factorizations (LU/Cholesky)
       * if possible or just outright try to use some other library that does fancy stuff
       * like UMFPACK. Would be also interesting to see if it could be optimized so we
       * store the factorization or update that instead of handling it individually. Another
       * point of interest would be to split loops out to make the solving faster, however,
       * this may be trickier to do (although it would surely let us use cholesky always if we
       * enforce that condition). */
      ret = cs_qrsol( 3, econ_G, X );
      if (ret != 1)
         WARN("Failed to solve the Economy System.");

      /*
       * Get the minimum and maximum to scale.
       */
      min = +HUGE_VALF;
      max = -HUGE_VALF;
      for (i=0; i<systems_nstack; i++) {
         if (X[i] < min)
            min = X[i];
         if (X[i] > max)
            max = X[i];
      }
      scale = 1. / (max - min);
      offset = 0.5 - min * scale;

      /*
       * I'm not sure I like the filtering of the results, but it would take
       * much more work to get a sane system working without the need of post
       * filtering.
       */
      for (i=0; i<systems_nstack; i++)
         for (k=0; k<systems_stack[i].nplanets; k++)
            systems_stack[i].planets[k]->commodities[j].price = planet_stack[i].commodities[j].base_price * (X[i] * scale + offset);
   }

   /* Clean up. */
   free(X);

   return 0;
}


/**
 * @brief Destroys the economy.
 */
void economy_destroy (void)
{
   int i;

   /* Must be initialized. */
   if (!econ_initialized)
      return;

   /* Clean up the prices in the planets stack. */
   for (i=0; i<planet_nstack; i++) {
      if (planet_stack[i].commodities != NULL) {
         free(planet_stack[i].commodities);
         planet_stack[i].commodities = NULL;
      }
   }

   /* Destroy the economy matrix. */
   if (econ_G != NULL) {
      cs_spfree( econ_G );
      econ_G = NULL;
   }

   /* Economy is now deinitialized. */
   econ_initialized = 0;
}
