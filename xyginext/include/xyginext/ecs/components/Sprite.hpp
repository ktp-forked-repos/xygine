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

#ifndef XY_SPRITE_HPP_
#define XY_SPRITE_HPP_

#include <xyginext/Config.hpp>

#include <SFML/Graphics/RenderStates.hpp>
#include <SFML/Graphics/Vertex.hpp>

#include <array>

namespace sf
{
    class Texture;
}

namespace xy
{
    /*!
    \brief Sprite component optimised to work with the ECS
    */
    class XY_EXPORT_API Sprite final
    {
    public:
        /*!
        \brief Default constructor
        */
        Sprite();

        /*!
        \brief Construct a sprite with a texture
        */
        explicit Sprite(const sf::Texture&);

        /*!
        \brief Sets the texture with which to draw this sprite.
        By default sprites are resized to the size of this texture
        */
        void setTexture(const sf::Texture&);

        /*!
        \brief Sets a sub rectangle of the current texture
        to be used when drawing.
        When a new texture is set on the sprite this is reset to
        the size of the texture,
        */
        void setTextureRect(sf::FloatRect);

        /*!
        \brief Sets the colour to be multiplied with this
        sprite when drawing.
        */
        void setColour(sf::Color);

        /*!
        \brief Sets the shader used when drawing this sprite.
        Passing nullptr removes any active shader.
        */
        void setShader(sf::Shader*);

        /*!
        \brief Sets the blend mode used when rendering this sprite
        */
        void setBlendMode(sf::BlendMode);

        /*!
        \brief Returns a pointer to the sprite's active texture
        */
        const sf::Texture* getTexture() const;

        /*!
        \brief Returns the current sub-rectangle used to crop the sprite
        */
        sf::FloatRect getTextureRect() const { return m_textureRect; }

        /*!
        \brief Returns the current sprite colour
        */
        sf::Color getColour() const;

        /*!
        \brief Returns a pointer to the active shader, if any
        */
        const sf::Shader* getShader() const;

        /*!
        \brief Returns the sprite's current blend mode
        */
        sf::BlendMode getBlendMode() const { return m_states.blendMode; }

        /*!
        \brief Returns the local (pre transform) bounds
        */
        sf::FloatRect getLocalBounds() const { return m_localBounds; }

    private:

        sf::FloatRect m_textureRect;
        sf::RenderStates m_states;
        std::array<sf::Vertex, 4u> m_vertices;
        sf::FloatRect m_localBounds;
        bool m_dirty;

        friend class SpriteRenderer;
    };
}

#endif //XY_SPRITE_HPP_