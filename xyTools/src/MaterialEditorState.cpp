/*********************************************************************
Matt Marchant 2014 - 2016
http://trederia.blogspot.com

xygine - Zlib license.

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

#include <MaterialEditorState.hpp>

#include <xygine/App.hpp>
#include <xygine/imgui/imgui.h>
#include <xygine/FileSystem.hpp>

MaterialEditorState::MaterialEditorState(xy::StateStack& stack, Context context)
    :xy::State  (stack, context),
    m_messageBus(context.appInstance.getMessageBus())
{
    launchLoadingScreen();
    buildMenu();
    quitLoadingScreen();
}

MaterialEditorState::~MaterialEditorState()
{
    xy::App::removeUserWindows(this);
}

//public
bool MaterialEditorState::update(float)
{
    return true;
}

void MaterialEditorState::draw()
{

}

bool MaterialEditorState::handleEvent(const sf::Event&)
{
    return true;
}

void MaterialEditorState::handleMessage(const xy::Message&)
{

}

//private
void MaterialEditorState::buildMenu()
{
    xy::Logger::log(xy::FileSystem::getCurrentDirectory(), xy::Logger::Type::Info);
    auto dirs = xy::FileSystem::listDirectories("/assets");
    for (const auto& str : dirs)
    {
        xy::Logger::log(str, xy::Logger::Type::Info);
    }
    
    xy::App::addUserWindow([]()
    {
        nim::Begin("Material Editor!");

        nim::End();
    }, this);
}