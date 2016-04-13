/* StonerView: An eccentric visual toy.
   Copyright 1998-2001 by Andrew Plotkin (erkyrath@eblong.com)
   http://www.eblong.com/zarf/stonerview.html
   This program is distributed under the GPL.
   See main.c, the Copying document, or the above URL for details.
*/

typedef struct elem_struct {
  GLfloat pos[3];
  GLfloat vervec[2];
  GLfloat col[4];
} elem_t;

extern elem_t elist[];

extern int init_move(void);
extern void final_move(void);
extern void move_increment(void);
