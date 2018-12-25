#include <cstring>
#include <fstream>
#include <SDL_assert.h>
#include <SDL_surface.h>
#include <rapidjson/document.h>
#include <rapidjson/istreamwrapper.h>
#include <ovis/core/log.hpp>
#include <ovis/core/resource_manager.hpp>
#include <ovis/graphics/cubemap.hpp>
#include <ovis/graphics/graphics_context.hpp>

namespace ovis {

Cubemap::Cubemap(GraphicsContext* context,
                 const CubemapDescription& description, const void* pixels)
    : Texture(context), m_description(description) {
  Bind(0);

  GLenum internal_format;
  GLenum source_format;
  GLenum source_type;
  switch (description.format) {
    case TextureFormat::RGB_UINT8:
      internal_format = GL_RGB;
      source_format = GL_RGB;
      source_type = GL_UNSIGNED_BYTE;
      break;

    case TextureFormat::RGBA_UINT8:
      internal_format = GL_RGBA;
      source_format = GL_RGBA;
      source_type = GL_UNSIGNED_BYTE;
      break;

    default:
      SDL_assert(false);
      break;
  }

  const size_t image_size = description.width * description.height * 3;

  for (int i = 0; i < 6; ++i) {
    const char* image_pixels = reinterpret_cast<const char*>(pixels);
    if (image_pixels != nullptr) {
      image_pixels += i * image_size;
    }
    glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, internal_format,
                 static_cast<GLsizei>(description.width),
                 static_cast<GLsizei>(description.height), 0, source_format,
                 source_type, image_pixels);
  }

  GLenum min_filter;
  GLenum mag_filter;
  switch (description.filter) {
    case TextureFilter::POINT:
      min_filter = description.mip_map_count > 1 ? GL_NEAREST_MIPMAP_NEAREST
                                                 : GL_NEAREST;
      mag_filter = GL_NEAREST;
      break;

    case TextureFilter::BILINEAR:
      min_filter =
          description.mip_map_count > 1 ? GL_LINEAR_MIPMAP_NEAREST : GL_LINEAR;
      mag_filter = GL_LINEAR;
      break;

    case TextureFilter::TRILINEAR:
      SDL_assert(description.mip_map_count > 1);
      min_filter = GL_LINEAR_MIPMAP_LINEAR;
      mag_filter = GL_LINEAR;
      break;

    default:
      SDL_assert(false);
      break;
  }
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, min_filter);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, mag_filter);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

void Cubemap::Write(CubemapSide side, std::size_t level, std::size_t x,
                    std::size_t y, std::size_t width, std::size_t height,
                    const void* data) {
  Bind(0);

  GLenum source_format;
  GLenum source_type;
  switch (m_description.format) {
    case TextureFormat::RGB_UINT8:
      source_format = GL_RGB;
      source_type = GL_UNSIGNED_BYTE;
      break;

    case TextureFormat::RGBA_UINT8:
      source_format = GL_RGBA;
      source_type = GL_UNSIGNED_BYTE;
      break;

    default:
      SDL_assert(false);
      break;
  }

  glTexSubImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + static_cast<int>(side),
                  static_cast<GLsizei>(level), static_cast<GLsizei>(x),
                  static_cast<GLsizei>(y), static_cast<GLsizei>(width),
                  static_cast<GLsizei>(height), source_format, source_type,
                  data);
}

void Cubemap::Bind(int texture_unit) {
  context()->BindTexture(GL_TEXTURE_CUBE_MAP, name(), texture_unit);
}

bool LoadCubemap(GraphicsContext* graphics_context,
                 ResourceManager* resource_manager,
                 const rapidjson::Document& parameters, const std::string& id,
                 const std::string& directory) {
  CubemapDescription cubemap_desc;
  cubemap_desc.width = parameters["width"].GetInt();
  cubemap_desc.height = parameters["height"].GetInt();
  cubemap_desc.mip_map_count = 1;

  const auto& filter = parameters["filter"].GetString();
  if (std::strcmp(filter, "point") == 0) {
    cubemap_desc.filter = TextureFilter::POINT;
  } else if (std::strcmp(filter, "bilinear") == 0) {
    cubemap_desc.filter = TextureFilter::BILINEAR;
  } else if (std::strcmp(filter, "trilinear") == 0) {
    cubemap_desc.filter = TextureFilter::TRILINEAR;
  } else {
    LogE("Failed to load cubemap '", id, "': invalid filter (", filter, ")");
    return false;
  }

  const auto& format = parameters["format"].GetString();
  if (std::strcmp(format, "RGB_UINT8") == 0) {
    cubemap_desc.format = TextureFormat::RGB_UINT8;
  } else if (std::strcmp(format, "RGBA_UINT8") == 0) {
    cubemap_desc.format = TextureFormat::RGBA_UINT8;
  } else {
    LogE("Failed to load cubemap '", id, "': invalid format (", format, ")");
    return false;
  }

  std::vector<uint8_t> buffer =
      LoadBinaryFile(directory + "/" + parameters["data_file"].GetString());
  resource_manager->RegisterResource<Cubemap>(id, graphics_context,
                                              cubemap_desc, buffer.data());

  LogI("Sucessfully loaded cubemap: ", id);
  return true;
}

}  // namespace ovis