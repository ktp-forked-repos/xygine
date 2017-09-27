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

#include "Server.hpp"
#include "MapData.hpp"
#include "PacketIDs.hpp"
#include "ActorSystem.hpp"
#include "PlayerSystem.hpp"
#include "AnimationController.hpp"
#include "CommandIDs.hpp"
#include "ServerMessages.hpp"
#include "CollisionSystem.hpp"
#include "Hitbox.hpp"
#include "ClientServerShared.hpp"
#include "sha1.hpp"
#include "BubbleSystem.hpp"
#include "NPCSystem.hpp"
#include "FruitSystem.hpp"
#include "MessageIDs.hpp"
#include "InventoryDirector.hpp"

#include <xyginext/ecs/components/Transform.hpp>
#include <xyginext/ecs/components/Callback.hpp>
#include <xyginext/ecs/components/CommandTarget.hpp>
#include <xyginext/ecs/components/QuadTreeItem.hpp>

#include <xyginext/ecs/systems/CallbackSystem.hpp>
#include <xyginext/ecs/systems/CommandSystem.hpp>
#include <xyginext/ecs/systems/QuadTree.hpp>

#include <xyginext/util/Random.hpp>
#include <xyginext/util/Vector.hpp>

#include <xyginext/core/FileSystem.hpp>

#include <tmxlite/Map.hpp>

#include <SFML/System/Clock.hpp>

#include <cstring>
#include <fstream>

#define CLIENT_MESSAGE(x) m_host.broadcastPacket(PacketID::ServerMessage, x, xy::NetFlag::Reliable, 1)

namespace
{
    const float tickRate = 1.f / 25.f;
    const float updateRate = 1.f / 60.f;
    const float endOfRoundTime = 6.f;
}

GameServer::GameServer()
    : m_ready       (false),
    m_running       (false),
    m_thread        (&GameServer::update, this),
    m_scene         (m_messageBus),
    m_currentMap    (0),
    m_mapTimer      (3.f)
{
    m_clients[0].data.actor.type = ActorID::PlayerOne;
    m_clients[0].data.spawnX = playerOneSpawn.x;
    m_clients[0].data.spawnY = playerOneSpawn.y;

    m_clients[1].data.actor.type = ActorID::PlayerTwo;
    m_clients[1].data.spawnX = playerTwoSpawn.x;
    m_clients[1].data.spawnY = playerTwoSpawn.y;
}

GameServer::~GameServer()
{
    stop();
}

//public
void GameServer::start()
{
    //we can log these locally, as we'd be in the same thread
    xy::Logger::log("Starting local server", xy::Logger::Type::Info);
    
    initMaplist();
    if (m_mapFiles.empty())
    {
        xy::Logger::log("No maps found in map directory", xy::Logger::Type::Error);
        return;
    }

    m_running = true;
    m_thread.launch();
}

void GameServer::stop()
{
    //xy::Logger::log("Stopping server", xy::Logger::Type::Info);
    CLIENT_MESSAGE(MessageIdent::StopServer);

    m_running = false;
    m_thread.wait();
    m_ready = false;
}

//private
void GameServer::update()
{
    sf::Clock clock;
    float tickAccumulator = 0.f;
    float updateAccumulator = 0.f;

    initScene();
    loadMap();
    m_ready = m_host.start("", 40003, 2, 2);

    while (m_running)
    {
        const float dt = clock.restart().asSeconds();
        tickAccumulator += dt;
        updateAccumulator += dt;

        while (updateAccumulator > updateRate)
        {
            updateAccumulator -= updateRate;

            //player inputs are broadcast at 60fps (ish) so we need to try to keep up
            xy::NetEvent evt;
            while (m_host.pollEvent(evt))
            {
                switch (evt.type)
                {
                case xy::NetEvent::ClientConnect:
                    handleConnect(evt);
                    break;
                case xy::NetEvent::ClientDisconnect:
                    handleDisconnect(evt);
                    break;
                case xy::NetEvent::PacketReceived:
                    handlePacket(evt);
                    break;
                default: break;
                }
            }

            //update scene logic.
            while (!m_messageBus.empty())
            {
                auto msg = m_messageBus.poll();
                handleMessage(msg);
                m_scene.forwardMessage(msg);
            }
            m_scene.update(updateRate);
        }

        //network updates are less frequent than logic updates
        while (tickAccumulator > tickRate)
        {
            tickAccumulator -= tickRate;

            //broadcast scene state - TODO assemble this into one large packet rather than many small?
            const auto& actors = m_scene.getSystem<ActorSystem>().getActors();
            for (const auto& actor : actors)
            {
                const auto& actorComponent = actor.getComponent<Actor>();
                const auto& tx = actor.getComponent<xy::Transform>().getPosition();
                const auto& anim = actor.getComponent<AnimationController>();

                ActorState state;
                state.actor.id = actorComponent.id;
                state.actor.type = actorComponent.type;
                state.x = tx.x;
                state.y = tx.y;
                state.serverTime = m_serverTime.getElapsedTime().asMilliseconds();
                state.animationDirection = anim.direction;
                state.animationID = anim.nextAnimation;

                m_host.broadcastPacket(PacketID::ActorUpdate, state, xy::NetFlag::Unreliable);
            }

            //send client reconciliation
            for (const auto& c : m_clients)
            {
                if (c.data.actor.id > -1)
                {
                    auto ent = m_scene.getEntity(c.data.actor.id);
                    const auto& tx = ent.getComponent<xy::Transform>().getPosition();
                    const auto& player = ent.getComponent<Player>();

                    ClientState state;
                    state.actor.id = c.data.actor.id;
                    state.actor.type = c.data.actor.type;
                    state.x = tx.x;
                    state.y = tx.y;
                    state.clientTime = player.history[player.lastUpdatedInput].timestamp;
                    state.playerState = player.state;
                    state.playerVelocity = player.velocity;
                    state.playerTimer = player.timer;

                    m_host.sendPacket(c.peer, PacketID::ClientUpdate, state, xy::NetFlag::Unreliable);
                }
            }
        }

        //check if it's time to change map
        checkMapStatus(dt);
    }

    m_host.stop();
}

void GameServer::handleConnect(const xy::NetEvent& evt)
{
    LOG("Client connected from " + evt.peer.getAddress(), xy::Logger::Type::Info);

    //TODO check not existing client

    //send map name, list of actor ID's up to count
    m_host.sendPacket(evt.peer, PacketID::MapJoin, m_mapData, xy::NetFlag::Reliable, 1);
}

void GameServer::handleDisconnect(const xy::NetEvent& evt)
{
    LOG("Client dropped from server", xy::Logger::Type::Info);
    const auto& peer = evt.peer;

    auto client = std::find_if(m_clients.begin(), m_clients.end(),
        [&peer](const Client& client)
    {
        return client.peer == peer;
    });

    if (client != m_clients.end())
    {
        //broadcast to clients
        m_host.broadcastPacket(PacketID::ClientDisconnected, client->data, xy::NetFlag::Reliable, 1);

        //remove from scene
        m_scene.destroyEntity(m_scene.getEntity(client->data.actor.id));

        //update the client array
        client->data.actor.id = ActorID::None;
        client->data.peerID = 0;
        client->peer = {};
    }
}

void GameServer::handlePacket(const xy::NetEvent& evt)
{
    switch (evt.packet.getID())
    {
    default: break;
        //if client loaded send initial positions
    case PacketID::ClientReady:
    {
        std::size_t playerNumber = 0;
        if (m_clients[0].data.actor.id != ActorID::None)
        {
            playerNumber = 1;
            //send existing client data
            m_host.sendPacket(evt.peer, PacketID::ClientData, m_clients[0].data, xy::NetFlag::Reliable, 1);
        }
        //add the player actor to the scene
        spawnPlayer(playerNumber);

        //send the client info
        m_clients[playerNumber].data.peerID = evt.peer.getID();
        m_clients[playerNumber].peer = evt.peer;
        m_clients[playerNumber].ready = true;
        m_host.broadcastPacket(PacketID::ClientData, m_clients[playerNumber].data, xy::NetFlag::Reliable, 1);

        //send initial position of existing actors
        const auto& actors = m_scene.getSystem<ActorSystem>().getActors();
        for (const auto& actor : actors)
        {
            const auto& actorComponent = actor.getComponent<Actor>();
            const auto& tx = actor.getComponent<xy::Transform>().getPosition();

            ActorState state;
            state.actor.id = actorComponent.id;
            state.actor.type = actorComponent.type;
            state.x = tx.x;
            state.y = tx.y;

            m_host.sendPacket(evt.peer, PacketID::ActorAbsolute, state, xy::NetFlag::Reliable, 1);
        }
    }
        break;
    case PacketID::ClientInput:
    {        
        auto ip = evt.packet.as<InputUpdate>();
        xy::Command cmd;
        cmd.targetFlags = (ip.playerNumber == 0) ? CommandID::PlayerOne : CommandID::PlayerTwo;
        cmd.action = [ip](xy::Entity entity, float)
        {            
            auto& player = entity.getComponent<Player>();

            //update player input history
            player.history[player.currentInput].mask = ip.input;
            player.history[player.currentInput].timestamp = ip.clientTime;
            player.currentInput = (player.currentInput + 1) % player.history.size();
        };
        m_scene.getSystem<xy::CommandSystem>().sendCommand(cmd);
    }
        break;
    case PacketID::MapReady:
    {
        sf::Int16 actor = evt.packet.as<sf::Int16>();
        if (actor == ActorID::PlayerOne)
        {
            m_clients[0].ready = true;
        }
        else if (actor == ActorID::PlayerTwo)
        {
            m_clients[1].ready = true;
        }
        beginNewRound();

    }
        break;
    }
}

void GameServer::checkMapStatus(float dt)
{
    m_mapTimer -= dt;

    if (m_mapData.actorCount == 0
        && m_mapTimer < 0)
    {
        //clear remaining actors (should just be collectables / bubbles)
        //as well as any geometry
        xy::Command cmd;
        cmd.targetFlags = CommandID::MapItem;
        cmd.action = [&](xy::Entity entity, float)
        {
            m_scene.destroyEntity(entity);
            //std::cout << "delete server geometry" << std::endl;
        };
        m_scene.getSystem<xy::CommandSystem>().sendCommand(cmd);
        m_scene.update(0.f); //force the command right away

        //load next map
        m_currentMap = (m_currentMap + 1) % m_mapFiles.size();
        loadMap(); //TODO we need to check this was successful

        //tell clients to do map change
        m_host.broadcastPacket(PacketID::MapChange, m_mapData, xy::NetFlag::Reliable, 1);

        //set clients to not ready
        m_clients[0].ready = false;
        m_clients[1].ready = false;

        /*cmd.targetFlags = CommandID::PlayerOne | CommandID::PlayerTwo;
        cmd.action = [&](xy::Entity entity, float)
        {
            auto number = entity.getComponent<Player>().playerNumber;
            entity.getComponent<xy::Transform>().setPosition(m_clients[number].data.spawnX, m_clients[number].data.spawnY);
            entity.getComponent<Player>().direction = (number == 0) ? Player::Direction::Right : Player::Direction::Left;
        };
        m_scene.getSystem<xy::CommandSystem>().sendCommand(cmd);*/

        m_scene.getSystem<NPCSystem>().setEnabled(false);
        m_scene.getSystem<CollisionSystem>().setEnabled(false);
    }
}

void GameServer::initMaplist()
{
    auto mapFiles = xy::FileSystem::listFiles("assets/maps");

    //if no map cycle list create it
    if (!xy::FileSystem::fileExists("assets/maps/mapcycle.txt"))
    {
        //TODO should really be using user data directory
        std::ofstream file("assets/maps/mapcycle.txt");
        if (file.good())
        {
            for (const auto& map : mapFiles)
            {
                file << map << std::endl;
            }
            file.close();
        }
    }

    static const std::size_t maxMaps = 255;
    std::ifstream file("assets/maps/mapcycle.txt");
    if (file.good())
    {
        std::string line;
        while (!file.eof() && m_mapFiles.size() < maxMaps)
        {
            std::getline(file, line);
            m_mapFiles.push_back(line);
        }
    }

    //remove from list if file doesn't exist
    m_mapFiles.erase(std::remove_if(m_mapFiles.begin(), m_mapFiles.end(),
        [&mapFiles](const std::string& str) 
    {
        return std::find(mapFiles.begin(), mapFiles.end(), str) == mapFiles.end();
    }), m_mapFiles.end());
}

void GameServer::initScene()
{
    m_scene.addSystem<xy::QuadTree>(m_messageBus, MapBounds);
    m_scene.addSystem<CollisionSystem>(m_messageBus, true);    
    m_scene.addSystem<ActorSystem>(m_messageBus);
    m_scene.addSystem<BubbleSystem>(m_messageBus, m_host);
    m_scene.addSystem<NPCSystem>(m_messageBus, m_host);
    m_scene.addSystem<FruitSystem>(m_messageBus, m_host);
    m_scene.addSystem<PlayerSystem>(m_messageBus, true);
    //m_scene.addSystem<xy::CallbackSystem>(m_messageBus);
    m_scene.addSystem<xy::CommandSystem>(m_messageBus);

    m_scene.addDirector<InventoryDirector>(m_host);
}

void GameServer::loadMap()
{
    tmx::Map map;
    if (map.load("assets/maps/" + m_mapFiles[m_currentMap]))
    {
        auto sha1 = getSha("assets/maps/" + m_mapFiles[m_currentMap]);

        std::strcpy(m_mapData.mapName, m_mapFiles[m_currentMap].c_str());
        std::strcpy(m_mapData.mapSha, sha1.c_str());

        //load collision geometry
        sf::Uint8 flags = 0;
        const auto& layers = map.getLayers();
        for (const auto& layer : layers)
        {
            if (layer->getType() == tmx::Layer::Type::Object)
            {
                //create map collision
                auto name = xy::Util::String::toLower(layer->getName());
                if (name == "platform")
                {
                    const auto& objs = dynamic_cast<tmx::ObjectGroup*>(layer.get())->getObjects();
                    for (const auto& obj : objs)
                    {
                        createCollisionObject(m_scene, obj, CollisionType::Platform);
                    }

                    flags |= MapFlags::Platform;
                }
                else if (name == "solid")
                {
                    const auto& objs = dynamic_cast<tmx::ObjectGroup*>(layer.get())->getObjects();
                    for (const auto& obj : objs)
                    {
                        createCollisionObject(m_scene, obj, CollisionType::Solid);
                    }
                    flags |= MapFlags::Solid;
                }
                else if (name == "teleport")
                {
                    const auto& objs = dynamic_cast<tmx::ObjectGroup*>(layer.get())->getObjects();
                    for (const auto& obj : objs)
                    {
                        createCollisionObject(m_scene, obj, CollisionType::Teleport);
                    }
                    flags |= MapFlags::Teleport;
                }
                else if (name == "spawn")
                {
                    sf::Int32 spawnCount = 0;
                    
                    const auto& objs = dynamic_cast<tmx::ObjectGroup*>(layer.get())->getObjects();
                    for (const auto& obj : objs)
                    {
                        auto name = xy::Util::String::toLower(obj.getName());
                        if (name == "whirlybob")
                        {
                            spawnNPC(ActorID::Whirlybob, { obj.getPosition().x, obj.getPosition().y });
                            spawnCount++;
                        }
                        else if (name == "clocksy")
                        {
                            spawnNPC(ActorID::Clocksy, { obj.getPosition().x, obj.getPosition().y });
                            spawnCount++;
                        }
                    }
                    //spawnNPC(ActorID::Clocksy, { 220.f, 220.f }); spawnCount++;
                    flags |= (spawnCount == 0) ? 0 : MapFlags::Spawn;
                }
            }
        }
        if (flags != MapFlags::Server)
        {
            CLIENT_MESSAGE(MessageIdent::MapFailed);
            //std::cout << m_mapFiles[m_currentMap] << ", Bad flags! " << std::bitset<8>(flags) << std::endl;
            return;
        }

        m_serverTime.restart();
    }
    else
    {
        //broadcast message ident that causes client to quit
        CLIENT_MESSAGE(MessageIdent::MapFailed);
        //std::cout << "failed opening next map" << std::endl;
    }
}

void GameServer::beginNewRound()
{
    //check if all connected clients are ready then send message to continue
    bool p1Ready = ((m_clients[0].data.actor.id == ActorID::None) || (m_clients[0].data.actor.id != ActorID::None && m_clients[0].ready));
    bool p2Ready = ((m_clients[1].data.actor.id == ActorID::None) || (m_clients[1].data.actor.id != ActorID::None && m_clients[1].ready));

    if (p1Ready && p2Ready)
    {
        m_scene.getSystem<NPCSystem>().setEnabled(true);
        m_scene.getSystem<CollisionSystem>().setEnabled(true);

        xy::Command cmd;
        cmd.targetFlags = CommandID::PlayerOne | CommandID::PlayerTwo;
        cmd.action = [](xy::Entity entity, float)
        {
            if (entity.getComponent<xy::CommandTarget>().ID == CommandID::PlayerOne)
            {
                entity.getComponent<xy::Transform>().setPosition(playerOneSpawn);
            }
            else
            {
                entity.getComponent<xy::Transform>().setPosition(playerTwoSpawn);
            }
        };
        m_scene.getSystem<xy::CommandSystem>().sendCommand(cmd);

        //send initial position of existing actors
        const auto& actors = m_scene.getSystem<ActorSystem>().getActors();
        for (const auto& actor : actors)
        {
            const auto& actorComponent = actor.getComponent<Actor>();
            const auto& tx = actor.getComponent<xy::Transform>().getPosition();

            ActorState state;
            state.actor.id = actorComponent.id;
            state.actor.type = actorComponent.type;
            state.x = tx.x;
            state.y = tx.y;

            m_host.broadcastPacket(PacketID::ActorAbsolute, state, xy::NetFlag::Reliable, 1);
        }
    }
}

sf::Int32 GameServer::spawnPlayer(std::size_t player)
{
    auto entity = m_scene.createEntity();
    entity.addComponent<Actor>().type = (player == 0) ? ActorID::PlayerOne : ActorID::PlayerTwo;
    entity.getComponent<Actor>().id = entity.getIndex();
    m_clients[player].data.actor = entity.getComponent<Actor>();
    entity.addComponent<xy::Transform>().setPosition(m_clients[player].data.spawnX, m_clients[player].data.spawnY);
    entity.getComponent<xy::Transform>().setOrigin(PlayerSize / 2.f, PlayerSize);

    entity.addComponent<CollisionComponent>().addHitbox({ PlayerSizeOffset, PlayerSizeOffset, PlayerSize, PlayerSize }, CollisionType::Player);
    entity.getComponent<CollisionComponent>().addHitbox({ -PlayerSizeOffset, PlayerSize + PlayerSizeOffset, PlayerSize + (PlayerSizeOffset * 2.f), PlayerFootSize }, CollisionType::Foot);
    entity.getComponent<CollisionComponent>().setCollisionCategoryBits(CollisionFlags::Player);
    entity.getComponent<CollisionComponent>().setCollisionMaskBits(CollisionFlags::PlayerMask);
    entity.addComponent<xy::QuadTreeItem>().setArea(entity.getComponent<CollisionComponent>().getLocalBounds());

    entity.addComponent<AnimationController>();

    //add client controller
    entity.addComponent<Player>().playerNumber = static_cast<sf::Uint8>(player);
    entity.getComponent<Player>().spawnPosition = entity.getComponent<xy::Transform>().getPosition();
    if (player == 1) entity.getComponent<Player>().direction = Player::Direction::Left;
    entity.addComponent<xy::CommandTarget>().ID = (player == 0) ? CommandID::PlayerOne : CommandID::PlayerTwo;

    //raise a message to say this happened
    auto* msg = m_messageBus.post<PlayerEvent>(MessageID::PlayerMessage);
    msg->type = PlayerEvent::Spawned;
    msg->entity = entity;

    return entity.getIndex();
}

void GameServer::spawnNPC(sf::Int32 id, sf::Vector2f pos)
{
    auto entity = m_scene.createEntity();
    entity.addComponent<xy::Transform>().setPosition(pos);
    entity.getComponent<xy::Transform>().setOrigin(NPCSize / 2.f, NPCSize / 2.f);
    entity.addComponent<Actor>().id = entity.getIndex();
    entity.getComponent<Actor>().type = id;

    entity.addComponent<CollisionComponent>().addHitbox({ 0.f, 0.f, NPCSize, NPCSize }, CollisionType::NPC);
    entity.getComponent<CollisionComponent>().setCollisionCategoryBits(CollisionFlags::NPC);
    entity.getComponent<CollisionComponent>().setCollisionMaskBits(CollisionFlags::NPCMask);
    entity.addComponent<xy::QuadTreeItem>().setArea({ 0.f, 0.f, NPCSize, NPCSize });

    entity.addComponent<AnimationController>();
    entity.addComponent<xy::CommandTarget>().ID = CommandID::MapItem;

    entity.addComponent<NPC>();
    switch (id)
    {
    default: break;
    case ActorID::Whirlybob:
        entity.getComponent<NPC>().velocity = 
        {
            xy::Util::Random::value(-1.f, 1.f),
            xy::Util::Random::value(-1.f, 1.f)
        };
        entity.getComponent<NPC>().velocity = xy::Util::Vector::normalise(entity.getComponent<NPC>().velocity);
        break;
    case ActorID::Clocksy:
        entity.getComponent<NPC>().velocity.x = (xy::Util::Random::value(1, 2) % 2 == 1) ? -1.f : 1.f;
        entity.getComponent<CollisionComponent>().addHitbox(
        { -PlayerSizeOffset, NPCSize,
            NPCSize + (PlayerSizeOffset * 2.f), PlayerFootSize }, CollisionType::Foot); //feets!
        break;
    }

    m_mapData.actors[m_mapData.actorCount++] = entity.getComponent<Actor>();

    auto* msg = m_messageBus.post<NpcEvent>(MessageID::NpcMessage);
    msg->type = NpcEvent::Spawned;
    msg->entityID = entity.getIndex();
}

void GameServer::handleMessage(const xy::Message& msg)
{
    switch (msg.id)
    {
    default: break;
    case MessageID::NpcMessage:
    {
        const auto& data = msg.getData<NpcEvent>();
        if (data.type == NpcEvent::Died)
        {
            //remove from list of actors
            std::size_t i = 0u;
            for (i; i < m_mapData.actorCount; ++i)
            {
                if (m_mapData.actors[i].id == data.entityID)
                {
                    break;
                }
            }

            m_mapData.actorCount--;
            m_mapData.actors[i] = m_mapData.actors[m_mapData.actorCount];

            //LOG(std::to_string(m_mapData.actorCount), xy::Logger::Type::Info);
            m_mapTimer = endOfRoundTime;
        }
    }
        break;
    }
}