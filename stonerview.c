/* StonerView: An eccentric visual toy.
   Copyright 1998-2001 by Andrew Plotkin (erkyrath@eblong.com)

   For the latest version, source code, and links to more of my stuff, see:
   http://www.eblong.com/zarf/stonerview.html

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program. (It should be a document entitled "Copying".) 
   If not, see the web URL above, or write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <GL/gl.h>

#include "general.h"
#include "move.h"
#include "view.h"

#define FRAMERATE (20) /* milliseconds per frame */

static void screenhack_usleep(unsigned long usecs);

int main(int argc, char *argv[])
{
  srand(time(NULL));

  if (!init_view(&argc, argv))
    return -1;
  if (!init_move())
    return -1;

  while (1) {
    win_draw();
    move_increment();
    screenhack_usleep(FRAMERATE * 1000L);
  }

  return 0;
}

static void screenhack_usleep(unsigned long usecs)
{
  struct timeval tv;
  tv.tv_sec  = usecs / 1000000L;
  tv.tv_usec = usecs % 1000000L;
  select(0, 0, 0, 0, &tv);
}
