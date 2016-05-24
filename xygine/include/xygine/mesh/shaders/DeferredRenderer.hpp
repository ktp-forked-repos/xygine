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

#ifndef XY_MESH_DEFERRED_HPP_
#define XY_MESH_DEFERRED_HPP_

#include <string>

namespace xy
{
    namespace Shader
    {
        namespace Mesh
        {
            const static std::string DeferredVertex =
                "in vec3 a_position;\n" \
                "in vec3 a_normal;\n" \

                "#if defined(TEXTURED) || defined(BUMP)\n" \
                "in vec2 a_texCoord0;\n" \
                "#endif\n" \

                "#if defined(BUMP)\n" \
                "in vec3 a_tangent;\n" \
                "in vec3 a_bitangent;\n" \
                "#endif\n" \

                "uniform mat4 u_worldMatrix;\n" \
                "uniform mat4 u_worldViewMatrix;\n" \
                "uniform mat3 u_normalMatrix;\n" \

                "layout (std140) uniform u_matrixBlock\n" \
                "{\n" \
                "    mat4 u_viewMatrix;\n" \
                "    mat4 u_projectionMatrix;\n" \
                "};\n" \

                "#if !defined(BUMP)\n"
                "out vec3 v_normalVector;\n" \
                "#else\n"
                "out mat3 v_tbn;\n" \
                "#endif\n"
                "out vec3 v_viewPosition;\n" \
                "#if defined(TEXTURED) || defined(BUMP)\n" \
                "out vec2 v_texCoord;\n" \
                "#endif\n" \

                "void main()\n" \
                "{\n" \
                "    vec4 viewPosition = u_worldViewMatrix * vec4(a_position, 1.0);\n" \
                "    v_viewPosition = (u_worldMatrix * vec4(a_position, 1.0)).xyz;//viewPosition.xyz;\n" \
                "    gl_Position = u_projectionMatrix * viewPosition;\n" \

                "#if defined(TEXTURED) || defined(BUMP)\n" \
                "    v_texCoord = a_texCoord0;\n" \
                "#endif\n" \

                "#if !defined(BUMP)\n"
                "    v_normalVector = u_normalMatrix * a_normal;\n" \
                "#else\n"
                "    mat3 normalMatrix = inverse(mat3(u_worldMatrix));\n" \
                "    vec3 t = normalize(normalMatrix * a_tangent);\n" \
                "    vec3 b = normalize(normalMatrix * a_bitangent);\n" \
                "    vec3 n = normalize(normalMatrix * a_normal);\n" \
                "    v_tbn = mat3(t, b, n);\n" \
                "#endif\n"
                "}";

            const static std::string DeferredFragment =
                "#if !defined(BUMP)\n" \
                "in vec3 v_normalVector;\n"
                "#else\n" \
                "in mat3 v_tbn;\n" \
                "#endif\n" \
                "in vec3 v_viewPosition;\n" \
                "#if defined(TEXTURED) || defined(BUMP)\n" \
                "in vec2 v_texCoord;\n" \
                "#endif\n" \

                "uniform float u_farPlane = 1500.0;\n" \
                "uniform vec4 u_colour = vec4(1.0);\n" \
                "#if defined(TEXTURED)\n"
                "uniform sampler2D u_diffuseMap;\n" \
                "#endif\n" \
                "#if defined(BUMP)\n"
                "uniform sampler2D u_normalMap;\n" \
                "#endif\n" \

                "out vec4[3] fragOut;\n" \

                "const float nearPlane = 0.1;\n" \
                "float lineariseDepth(float val)\n" \
                "{\n" \
                "    float z = val * 2.0 - 1.0;\n" \
                "    return (2.0 * nearPlane * u_farPlane) / (u_farPlane + nearPlane - z * (u_farPlane - nearPlane));\n" \
                "}\n" \

                "void main()\n" \
                "{\n" \
                "#if defined(TEXTURED)\n" \
                "    fragOut[0] = texture(u_diffuseMap, v_texCoord) * u_colour;\n" \
                "#else\n" \
                "    fragOut[0] = u_colour;\n" \
                "#endif\n" \

                "#if !defined(BUMP)\n" \
                "    fragOut[1] = vec4(normalize(v_normalVector), 1.0);\n" \
                "#else\n" \
                "    vec3 normal = texture(u_normalMap, v_texCoord).rgb * 2.0 - 1.0;\n" \
                "    fragOut[1] = vec4(normalize(v_tbn * normal).grb, 1.0);\n" \
                "#endif\n"

                "    fragOut[2].rgb = v_viewPosition;\n" \
                "    fragOut[2].a = lineariseDepth(gl_FragCoord.z);\n" \
                "}";
        }
    }
}

#define DEFERRED_COLOURED_VERTEX "#version 150\n" + xy::Shader::Mesh::DeferredVertex
#define DEFERRED_COLOURED_FRAGMENT "#version 150\n" + xy::Shader::Mesh::DeferredFragment

#define DEFERRED_COLOURED_BUMPED_VERTEX "#version 150\n#define BUMP\n" + xy::Shader::Mesh::DeferredVertex
#define DEFERRED_COLOURED_BUMPED_FRAGMENT "#version 150\n#define BUMP\n" + xy::Shader::Mesh::DeferredFragment

#define DEFERRED_TEXTURED_VERTEX "#version 150\n#define TEXTURED\n" + xy::Shader::Mesh::DeferredVertex
#define DEFERRED_TEXTURED_FRAGMENT "#version 150\n#define TEXTURED\n" + xy::Shader::Mesh::DeferredFragment

#define DEFERRED_TEXTURED_BUMPED_VERTEX "#version 150\n#define BUMP\n#define TEXTURED\n" + xy::Shader::Mesh::DeferredVertex
#define DEFERRED_TEXTURED_BUMPED_FRAGMENT "#version 150\n#define BUMP\n#define TEXTURED\n" + xy::Shader::Mesh::DeferredFragment

#endif //XY_MESH_DEFERRED_HPP_