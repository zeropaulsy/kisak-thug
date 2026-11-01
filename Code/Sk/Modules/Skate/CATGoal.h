// Create-A-Trick goal!

#ifndef __SK_MODULES_SKATE_CATGOAL_H__
#define __SK_MODULES_SKATE_CATGOAL_H__

#ifndef __CORE_DEFINES_H
    #include <Core/Defines.h>
#endif

#include <Sk/Modules/Skate/GoalManager.h>

namespace Game
{

class CCatGoal : public CGoal
{

public:
						CCatGoal( Script::CStruct* pParams );
	virtual				~CCatGoal();

	bool				Activate();
	bool				Deactivate( bool force = false, bool affect_tree = true );
protected:
};

}

#endif // #ifndef __SK_MODULES_SKATE_FINDGAPSGOAL_H__
