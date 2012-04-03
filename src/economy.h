/*
 * See Licensing and Copyright notice in naev.h
 */



#ifndef ECONOMY_H
#  define ECONOMY_H


#include <stdint.h>

#include "nlua.h"
#include "nluadef.h"

#define ECON_CRED_STRLEN      32 /**< Maximum length a credits2str string can reach. */

typedef int64_t credits_t;
#define CREDITS_MAX        INT64_MAX
#define CREDITS_MIN        INT64_MIN
#define CREDITS_PRI        PRIu64

/**
 * @struct Commodity
 *
 * @brief Represents a commodity.
 */
typedef struct Commodity_ {
   char* name; /**< Name of the commodity. */
   char* description; /**< Description of the commodity. */
   lua_State *lua; /**< Lua script. */
   /* Prices. */
   int supply;
   int demand;
   double base_price; /**< Base price of the commodity. */
   double price; /**< Actual price of the commodity. */
} Commodity;

/* commodity stack */
Commodity* commodity_stack; /**< Contains all the commodities. */
int commodity_nstack; /**< Number of commodities in the stack. */

/*
 * Commodity stuff.
 */
Commodity* commodity_get( const char* name );
Commodity* commodity_getW( const char* name );
int commodity_load (void);
void commodity_free (void);


/*
 * Economy stuff.
 */
int economy_init (void);
int economy_update (void);
int economy_refresh (void);
void economy_destroy (void);


/*
 * Misc stuff.
 */
void credits2str( char *str, credits_t credits, int decimals );
void commodity_Jettison( int pilot, Commodity* com, int quantity );
int commodity_compareTech( const void *commodity1, const void *commodity2 );


#endif /* ECONOMY_H */
