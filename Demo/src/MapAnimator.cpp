/*********************************************************************
(c) Matt Marchant 2017
http://trederia.blogspot.com

xygineXT - Zlib license.

This software is provided 'as-is', without any express or
implied warranty. In no event will the authors be held
liable for any damages arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute
it freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented;
you must not claim that you wrote the original software.
If you use this software in a product, an acknowledgment
in the product documentation would be appreciated but
is not required.

2. Altered source versions must be plainly marked as such,
and must not be misrepresented as being the original software.

3. This notice may not be removed or altered from any
source distribution.
*********************************************************************/

#include "MapAnimator.hpp"
#include "MessageIDs.hpp"
#include "CommandIDs.hpp"
#include "ClientServerShared.hpp"

#include <xyginext/ecs/components/Transform.hpp>
#include <xyginext/ecs/components/CommandTarget.hpp>
#include <xyginext/util/Vector.hpp>
#include <xyginext/core/App.hpp>
#include <xyginext/ecs/Scene.hpp>

namespace
{
    const float minDist = 1000.f; //sqr len
    const float speed = 500.f;
}

MapAnimatorSystem::MapAnimatorSystem(xy::MessageBus& mb)
    : xy::System(mb, typeid(MapAnimatorSystem))
{
    requireComponent<MapAnimator>();
    requireComponent<xy::Transform>();
}

void MapAnimatorSystem::process(float dt)
{
    std::size_t count = 0;
    
    auto& entities = getEntities();
    for (auto entity : entities)
    {
        auto& animator = entity.getComponent<MapAnimator>();
        auto& tx = entity.getComponent<xy::Transform>();

        if (animator.state == MapAnimator::State::Active)
        {
            auto dist = animator.dest - tx.getPosition();
            auto l2 = xy::Util::Vector::lengthSquared(dist);
            
            if (l2 < minDist)
            {
                tx.setPosition(animator.dest);
                animator.state = MapAnimator::State::Static;
            }
            else if (l2 > 0)
            {
                tx.move(xy::Util::Vector::normalise(dist) * speed * dt);

                if (l2 < minDist * 2.f) //we assume this is a map movement and tell player sprites to start moving
                {
                    xy::Command cmd;
                    cmd.targetFlags = CommandID::PlayerOne | CommandID::PlayerTwo;
                    cmd.action = [&](xy::Entity entity, float)
                    {
                        auto& animator = entity.getComponent<MapAnimator>();
                        animator.dest = entity.getComponent<xy::CommandTarget>().ID == CommandID::PlayerOne ? playerOneSpawn : playerTwoSpawn;
                        animator.state = MapAnimator::State::Active;
                    };
                    getScene()->getSystem<xy::CommandSystem>().sendCommand(cmd);
                }
            }

            //DPRINT("L2", std::to_string(l2));
        }
        else
        {
            count++;
        }
    }

    if (count != m_lastCount && count == entities.size())
    {
        //raise message to say all finished
        auto* msg = postMessage<MapEvent>(MessageID::MapMessage);
        msg->type = MapEvent::AnimationComplete;
    }
    m_lastCount = count;
}