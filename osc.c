/* StonerView: An eccentric visual toy.
   Copyright 1998-2001 by Andrew Plotkin (erkyrath@eblong.com)
   http://www.eblong.com/zarf/stonerview.html
   This program is distributed under the GPL.
   See main.c, the Copying document, or the above URL for details.
*/

#include <stdio.h>
#include <stdlib.h>
#include "general.h"
#include "osc.h"

/* A private linked list of all osc_t objects created. New objects are added
   to the end of the list, not the beginning. */
static osc_t *oscroot = NULL;
static osc_t **osctail = &oscroot;

static int rand_range(int min, int max);

/* Create a new, blank osc_t. The caller must fill in the type data. */
static osc_t *create_osc(int type)
{
  osc_t *osc = (osc_t *)malloc(sizeof(osc_t));
  if (!osc) 
    return NULL;
        
  osc->type = type;
  osc->next = NULL;
    
  *osctail = osc;
  osctail = &(osc->next);
    
  return osc;
}

/* Creation functions for all the osc_t types. These are all pretty obvious
   in their construction. */

osc_t *new_osc_constant(int val)
{
  osc_t *osc = create_osc(otyp_Constant);
  if (!osc)
    return NULL;
        
  osc->u.oconstant.val = val;
  return osc;
}

osc_t *new_osc_bounce(int min, int max, int step)
{
  int diff;
  osc_t *osc = create_osc(otyp_Bounce);
  if (!osc)
    return NULL;
        
  osc->u.obounce.min = min;
  osc->u.obounce.max = max;
  osc->u.obounce.step = step;
    
  /* Pick a random initial value between min and max. */
  if (step < 0)
    step = (-step);
  diff = (max-min) / step;
  osc->u.obounce.val = min + step * rand_range(0, diff-1);
    
  return osc;
}

osc_t *new_osc_wrap(int min, int max, int step)
{
  int diff;
  osc_t *osc = create_osc(otyp_Wrap);
  if (!osc)
    return NULL;
        
  osc->u.owrap.min = min;
  osc->u.owrap.max = max;
  osc->u.owrap.step = step;
    
  /* Pick a random initial value between min and max. */
  if (step < 0)
    step = (-step);
  diff = (max-min) / step;
  osc->u.owrap.val = min + step * rand_range(0, diff-1);
    
  return osc;
}

osc_t *new_osc_velowrap(int min, int max, osc_t *step)
{
  osc_t *osc = create_osc(otyp_VeloWrap);
  if (!osc)
    return NULL;
        
  osc->u.ovelowrap.min = min;
  osc->u.ovelowrap.max = max;
  osc->u.ovelowrap.step = step;
    
  /* Pick a random initial value between min and max. */
  osc->u.ovelowrap.val = rand_range(min, max);
    
  return osc;
}

osc_t *new_osc_multiplex(osc_t *sel, osc_t *ox0, osc_t *ox1, osc_t *ox2, osc_t *ox3)
{
  osc_t *osc = create_osc(otyp_Multiplex);
  if (!osc)
    return NULL;
    
  osc->u.omultiplex.sel = sel;
  osc->u.omultiplex.val[0] = ox0;
  osc->u.omultiplex.val[1] = ox1;
  osc->u.omultiplex.val[2] = ox2;
  osc->u.omultiplex.val[3] = ox3;
    
  return osc;
}

osc_t *new_osc_phaser(int phaselen)
{
  osc_t *osc = create_osc(otyp_Phaser);
  if (!osc)
    return NULL;
        
  osc->u.ophaser.phaselen = phaselen;

  osc->u.ophaser.count = 0;
  /* Pick a random phase to start in. */
  osc->u.ophaser.curphase = rand_range(0, NUM_PHASES-1);

  return osc;
}

osc_t *new_osc_randphaser(int minphaselen, int maxphaselen)
{
  osc_t *osc = create_osc(otyp_RandPhaser);
  if (!osc)
    return NULL;
        
  osc->u.orandphaser.minphaselen = minphaselen;
  osc->u.orandphaser.maxphaselen = maxphaselen;

  osc->u.orandphaser.count = 0;
  /* Pick a random phaselen to start with. */
  osc->u.orandphaser.curphaselen = rand_range(minphaselen, maxphaselen);
  /* Pick a random phase to start in. */
  osc->u.orandphaser.curphase = rand_range(0, NUM_PHASES-1);

  return osc;
}

osc_t *new_osc_linear(osc_t *base, osc_t *diff)
{
  osc_t *osc = create_osc(otyp_Linear);
  if (!osc)
    return NULL;

  osc->u.olinear.base = base;
  osc->u.olinear.diff = diff;
    
  return osc;
}

osc_t *new_osc_buffer(osc_t *val)
{
  int ix;
  osc_t *osc = create_osc(otyp_Buffer);
  if (!osc)
    return NULL;
    
  osc->u.obuffer.val = val;
  osc->u.obuffer.firstel = NUM_ELS-1;
    
  /* The last N values are stored in a ring buffer, which we must initialize
     here. */
  for (ix=0; ix<NUM_ELS; ix++) {
    osc->u.obuffer.el[ix] = osc_get(val, 0);
  }

  return osc;
}

/* Compute f(i,el) for the current i. */
int osc_get(osc_t *osc, int el)
{
  if (!osc)
    return 0;
        
  switch (osc->type) {
    
  case otyp_Constant:
    return osc->u.oconstant.val;
            
  case otyp_Bounce:
    return osc->u.obounce.val;
            
  case otyp_Wrap:
    return osc->u.owrap.val;
        
  case otyp_VeloWrap:
    return osc->u.ovelowrap.val;
        
  case otyp_Linear:
    return osc_get(osc->u.olinear.base, el)
      + el * osc_get(osc->u.olinear.diff, el);
        
  case otyp_Multiplex: {
    struct omultiplex_struct *ox = &(osc->u.omultiplex);
    int sel = osc_get(ox->sel, el);
    return osc_get(ox->val[sel % NUM_PHASES], el);
  }
        
  case otyp_Phaser: {
    struct ophaser_struct *ox = &(osc->u.ophaser);
    return ox->curphase;
  }
        
  case otyp_RandPhaser: {
    struct orandphaser_struct *ox = &(osc->u.orandphaser);
    return ox->curphase;
  }
        
  case otyp_Buffer: {
    struct obuffer_struct *ox = &(osc->u.obuffer);
    return ox->el[(ox->firstel + el) % NUM_ELS];
  }
        
  default:
    return 0;
  }
}

/* Increment i. This affects all osc_t objects; we go down the linked list to
   get them all. */
void osc_increment()
{
  osc_t *osc;
    
  for (osc = oscroot; osc; osc = osc->next) {
    switch (osc->type) {
        
    case otyp_Bounce: {
      struct obounce_struct *ox = &(osc->u.obounce);
      ox->val += ox->step;
      if (ox->val < ox->min && ox->step < 0) {
	ox->step = -(ox->step);
	ox->val = ox->min + (ox->min - ox->val);
      }
      if (ox->val > ox->max && ox->step > 0) {
	ox->step = -(ox->step);
	ox->val = ox->max + (ox->max - ox->val);
      }
      break;
    }
                
    case otyp_Wrap: {
      struct owrap_struct *ox = &(osc->u.owrap);
      ox->val += ox->step;
      if (ox->val < ox->min && ox->step < 0) {
	ox->val += (ox->max - ox->min);
      }
      if (ox->val > ox->max && ox->step > 0) {
	ox->val -= (ox->max - ox->min);
      }
      break;
    }
            
    case otyp_VeloWrap: {
      struct ovelowrap_struct *ox = &(osc->u.ovelowrap);
      int diff = (ox->max - ox->min);
      ox->val += osc_get(ox->step, 0);
      while (ox->val < ox->min)
	ox->val += diff;
      while (ox->val > ox->max)
	ox->val -= diff;
      break;
    }
            
    case otyp_Phaser: {
      struct ophaser_struct *ox = &(osc->u.ophaser);
      ox->count++;
      if (ox->count >= ox->phaselen) {
	ox->count = 0;
	ox->curphase++;
	if (ox->curphase >= NUM_PHASES)
	  ox->curphase = 0;
      }
      break;
    }
            
    case otyp_RandPhaser: {
      struct orandphaser_struct *ox = &(osc->u.orandphaser);
      ox->count++;
      if (ox->count >= ox->curphaselen) {
	ox->count = 0;
	ox->curphaselen = rand_range(ox->minphaselen, ox->maxphaselen);
	ox->curphase++;
	if (ox->curphase >= NUM_PHASES)
	  ox->curphase = 0;
      }
      break;
    }
            
    case otyp_Buffer: {
      struct obuffer_struct *ox = &(osc->u.obuffer);
      ox->firstel--;
      if (ox->firstel < 0)
	ox->firstel += NUM_ELS;
      ox->el[ox->firstel] = osc_get(ox->val, 0);
      /* We can assume that ox->val has already been incremented, since it
	 was created first. This is why new objects are put on the end
	 of the linked list... yeah, it's gross. */
      break;
    }
            
    default:
      break;
    }
  }
}

/* Return a random number between min and max, inclusive. */
static int rand_range(int min, int max)
{
  int res;
  unsigned int diff = (max+1) - min;
  if (diff <= 1)
    return min;
  res = rand() % diff;
  return min+res;
}

