/*
** License Applicability. Except to the extent portions of this file are
** made subject to an alternative license as permitted in the SGI Free
** Software License B, Version 1.1 (the "License"), the contents of this
** file are subject only to the provisions of the License. You may not use
** this file except in compliance with the License. You may obtain a copy
** of the License at Silicon Graphics, Inc., attn: Legal Services, 1600
** Amphitheatre Parkway, Mountain View, CA 94043-1351, or at:
** 
** http://oss.sgi.com/projects/FreeB
** 
** Note that, as provided in the License, the Software is distributed on an
** "AS IS" basis, with ALL EXPRESS AND IMPLIED WARRANTIES AND CONDITIONS
** DISCLAIMED, INCLUDING, WITHOUT LIMITATION, ANY IMPLIED WARRANTIES AND
** CONDITIONS OF MERCHANTABILITY, SATISFACTORY QUALITY, FITNESS FOR A
** PARTICULAR PURPOSE, AND NON-INFRINGEMENT.
** 
** Original Code. The Original Code is: OpenGL Sample Implementation,
** Version 1.2.1, released January 26, 2000, developed by Silicon Graphics,
** Inc. The Original Code is Copyright (c) 1991-2000 Silicon Graphics, Inc.
** Copyright in any portions created by third parties is as indicated
** elsewhere herein. All Rights Reserved.
** 
** Additional Notice Provisions: The application programming interfaces
** established by SGI in conjunction with the Original Code are The
** OpenGL(R) Graphics System: A Specification (Version 1.2.1), released
** April 1, 1999; The OpenGL(R) Graphics System Utility Library (Version
** 1.3), released November 4, 1998; and OpenGL(R) Graphics with the X
** Window System(R) (Version 1.3), released October 19, 1998. This software
** was created using the OpenGL(R) version 1.2.1 Sample Implementation
** published by SGI, but has not been independently verified as being
** compliant with the OpenGL(R) version 1.2.1 Specification.
**
** $Date$ $Revision: 1.1 $
*/
/*
** $Header: /cygdrive/c/RCVS/CVS/ReactOS/reactos/lib/glu32/libnurbs/nurbtess/sampledLine.h,v 1.1 2004/02/02 16:39:15 navaraf Exp $
*/

#ifndef _SAMPLEDLINE_H
#define _SAMPLEDLINE_H

#include "definitions.h"

class sampledLine{
  Int npoints;
  Real2 *points;

public:
  sampledLine(Int n_points);
  sampledLine(Int n_points, Real  pts[][2]);
  sampledLine(Real pt1[2], Real pt2[2]);
  sampledLine(); //special, careful about memory
  ~sampledLine();

  void init(Int n_points, Real2 *pts);//special, careful about memory

  void setPoint(Int i, Real p[2]) ;

  sampledLine* insert(sampledLine *nline);
  void deleteList();

  Int get_npoints() {return npoints;}
  Real2* get_points() {return points;}

  //u_reso is number of segments (may not be integer) per unit u
  void tessellate(Real u_reso, Real v_reso);//n segments
  void tessellateAll(Real u_reso, Real v_reso);

  void print();
  
  sampledLine* next;
};




#endif
