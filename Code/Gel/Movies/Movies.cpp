/*****************************************************************************
**																			**
**			              Neversoft Entertainment			                **
**																		   	**
**				   Copyright (C) 1999 - All Rights Reserved				   	**
**																			**
******************************************************************************
**																			**
**	Project:		GEL (Game Engine Library)								**
**																			**
**	Module:			GEL					 									**
**																			**
**	File name:		movies.cpp												**
**																			**
**	Created:		5/14/1	-	mjd											**
**																			**
**	Description:	streaming movies										**
**																			**
*****************************************************************************/


/*****************************************************************************
**							  	  Includes									**
*****************************************************************************/

#include <Core/Defines.h>
#include <Core/macros.h>
#include <Core/singleton.h>

#ifdef __PLAT_NGPS__
#include <Gel/Movies/Ngps/p_movies.h>
#elif defined( __PLAT_XBOX__ )
#include <Gel/Movies/Xbox/p_movies.h>
#elif defined( __PLAT_NGC__ )
#include <Gel/Movies/ngc/p_movies.h>
#endif

#include <Sys/Profiler.h>


/*****************************************************************************
**								DBG Information								**
*****************************************************************************/



namespace Flx
{



void PlayMovie( const char *pMovieName )
{
	return; // lwss: stubbed for now..
	//PMovies_PlayMovie( pMovieName );
}

}  // namespace Flx
