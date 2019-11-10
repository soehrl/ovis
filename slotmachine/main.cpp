#include <cmath>
#include <iostream>
#include <random>
#include <SDL_assert.h>
#include <glm/gtx/string_cast.hpp>
#include <ovis/gui/gui_controller.hpp>
#include <ovis/gui/gui_renderer.hpp>
#include <ovis/math/marching_cubes.hpp>
#include <ovis/core/file.hpp>
#include <ovis/core/log.hpp>
#include <ovis/core/range.hpp>
#include <ovis/graphics/cubemap.hpp>
#include <ovis/graphics/graphics_context.hpp>
#include <ovis/graphics/shader_program.hpp>
#include <ovis/graphics/static_mesh.hpp>
#include <ovis/graphics/vertex_buffer.hpp>
#include <ovis/graphics/vertex_input.hpp>
#include <ovis/scene/scene.hpp>
#include <ovis/scene/scene_controller.hpp>
#include <ovis/scene/scene_renderer.hpp>
#include <ovis/engine/engine.hpp>
#include <ovis/engine/window.hpp>

using namespace ovis;

class WheelsController : public SceneController {
 public:
  WheelsController(Scene* scene)
      : SceneController(scene, "WheelsController"),
        generator_(random_device_()) {}

  inline float GetWheelPosition(int index) const {
    return wheel_positions_[index];
  }

  void Update(std::chrono::microseconds delta_time) override {
    const float delta_time_seconds = delta_time.count() / 1000000.0f;

    switch (current_state_) {
      case State::ALL_SPINNING:
        wheel_positions_[0] += WHEEL_SPEED * delta_time_seconds;
      case State::FIRST_SET:
        wheel_positions_[1] += WHEEL_SPEED * delta_time_seconds;
      case State::SECOND_SET:
        wheel_positions_[2] += WHEEL_SPEED * delta_time_seconds;
      default:
        break;
    };

    if (timer_value_ <= delta_time.count()) {
      timer_value_ = 0;
      if (current_state_ == State::ALL_SPINNING) {
        current_state_ = State::FIRST_SET;
        wheel_positions_[0] = std::round(wheel_positions_[0]);
        StartTimer();
      } else if (current_state_ == State::FIRST_SET) {
        current_state_ = State::SECOND_SET;
        wheel_positions_[1] = std::round(wheel_positions_[1]);
        StartTimer();
      } else if (current_state_ == State::SECOND_SET) {
        current_state_ = State::ALL_SET;
        wheel_positions_[2] = std::round(wheel_positions_[2]);
        StartTimer();
      }
    } else {
      timer_value_ -= delta_time.count();
      LogD("Timer remaining ", timer_value_);
    }

    for (auto i : IRange(3)) {
      wheel_positions_[i] = fmod(wheel_positions_[i], ICON_COUNT);
    }
  }

  bool ProcessEvent(const SDL_Event& event) override {
    if (event.type == SDL_KEYDOWN &&
        event.key.keysym.scancode == SDL_SCANCODE_SPACE) {
      if (current_state_ == State::ALL_SET) {
        current_state_ = State::ALL_SPINNING;
        StartTimer();
        return true;
      }
    }

    return false;
  }

  void StartTimer() {
    std::uniform_int_distribution dist{1000000, 3000000};
    timer_value_ = dist(generator_);
    LogD("Timer set to ", timer_value_);
  }

 private:
  float wheel_positions_[3] = {0.0f, 0.0f, 0.0f};
  static constexpr float WHEEL_SPEED = 20.0f;  // Icons per second
  static constexpr int ICON_COUNT = 4;

  enum class State { ALL_SPINNING, FIRST_SET, SECOND_SET, ALL_SET };
  State current_state_ = State::ALL_SET;

  std::random_device random_device_;
  std::mt19937_64 generator_;

  int timer_value_ = 0;
};

class IconsRenderer : public SceneRenderer {
 public:
  IconsRenderer(Scene* scene, WheelsController* wheels_controller)
      : SceneRenderer(scene, "IconsRenderer"),
        wheels_controller_(wheels_controller) {}

  void CreateResources() override {
    ovis::SimpleMeshDescription mesh_desc;
    mesh_desc.primitive_topology = ovis::PrimitiveTopology::TRIANGLE_STRIP;
    mesh_desc.vertex_count = 4;
    mesh_ = std::make_unique<SpriteMesh>(context(), mesh_desc);
    mesh_->vertices()[0].position = glm::vec2(-0.5f, 0.5f);
    mesh_->vertices()[1].position = glm::vec2(-0.5f, -0.5f);
    mesh_->vertices()[2].position = glm::vec2(0.5f, 0.5f);
    mesh_->vertices()[3].position = glm::vec2(0.5f, -0.5f);
    mesh_->UpdateVertexBuffer();

    shader_program_ =
        scene()->resource_manager()->Load<ovis::ShaderProgram>("sprite.shader");

    icons_ = {
        scene()->resource_manager()->Load<Texture2D>("icons/beer.texture2d"),
        scene()->resource_manager()->Load<Texture2D>("icons/push_up.texture2d"),
        scene()->resource_manager()->Load<Texture2D>("icons/shot.texture2d"),
        scene()->resource_manager()->Load<Texture2D>("icons/squat.texture2d"),
    };
  }

  void Render() override {
    context()->Clear();

    const auto view_projection_matrix =
        scene()->camera().CalculateViewProjectionMatrix();

    RenderWheel(-400.0f, wheels_controller_->GetWheelPosition(0),
                view_projection_matrix);
    RenderWheel(0.0f, wheels_controller_->GetWheelPosition(1),
                view_projection_matrix);
    RenderWheel(400.0f, wheels_controller_->GetWheelPosition(2),
                view_projection_matrix);

    // float closest = std::round(wheel_position_);
    // --delay;
    // if (delay <= 0) {
    //   wheel_speed_ = 0.0f;
    //   wheel_position_ = closest;
    // } else {
    //   wheel_position_ = fmod(wheel_position_ + wheel_speed_, icons_.size());
    // }
  }

  void RenderWheel(float x_offset, float wheel_position,
                   const glm::mat4& view_projection_matrix) {
    const float icon_size = 200;
    const float margin = 50;
    const float total_size = icons_.size() * (icon_size + margin);

    const int num_images = 8;  // Should be power of two!
    const float factor = 1.0f / num_images;
    for (auto i : IRange(num_images)) {
      for (const auto icon : IndexRange(icons_.begin(), icons_.end())) {
        Transform sprite_transform;
        sprite_transform.SetScale(icon_size);

        (void)i;
        const float temp_wheel_position =
            wheel_position;  // - static_cast<float>(i) / num_images *
                             // wheel_speed_;
        float position =
            (static_cast<float>(icon.index()) - temp_wheel_position) *
            (icon_size + margin);
        if (position < total_size * -0.5) {
          position += total_size;
        }
        if (position > total_size * 0.5) {
          position -= total_size;
        }
        sprite_transform.Translate(glm::vec3(x_offset, position, 0.0f));

        shader_program_->SetUniform(
            "Transform",
            view_projection_matrix * sprite_transform.CalculateMatrix());
        shader_program_->SetTexture("SpriteTexture", icon.value().get());

        DrawItem draw_configuration;
        draw_configuration.shader_program = shader_program_.get();
        draw_configuration.blend_state.enabled = true;
        draw_configuration.blend_state.color_function = BlendFunction::ADD;
        draw_configuration.blend_state.source_color_factor =
            SourceBlendFactor::CONSTANT_COLOR;
        draw_configuration.blend_state.constant_color[0] = factor;
        draw_configuration.blend_state.constant_color[1] = factor;
        draw_configuration.blend_state.constant_color[2] = factor;
        draw_configuration.blend_state.constant_color[3] = 1.0f;
        draw_configuration.blend_state.destination_color_factor =
            DestinationBlendFactor::ONE;
        mesh_->Draw(draw_configuration);
      }
    }
  }

 private:
  using SpriteMesh = SimpleMesh<VertexAttribute::POSITION2D>;
  WheelsController* wheels_controller_;
  std::unique_ptr<SpriteMesh> mesh_;
  ovis::ResourcePointer<ovis::ShaderProgram> shader_program_;
  std::vector<ovis::ResourcePointer<ovis::Texture2D>> icons_;
};

class GameScene : public Scene {
 public:
  GameScene()
      : Scene("GameScene", {1280, 720}),
        wheels_controller_(this),
        icons_renderer_(this, &wheels_controller_) {
    camera().SetProjectionType(ProjectionType::ORTHOGRAPHIC);
    camera().SetVerticalFieldOfView(720);
    // camera().SetAspectRadio(720.0f / 1280.0f);
    camera().SetNearClipPlane(0.0f);
    camera().SetFarClipPlane(1.0f);
  }

 private:
  WheelsController wheels_controller_;
  IconsRenderer icons_renderer_;
};

int main(int argc, char* argv[]) {
  (void)argc;
  (void)argv;
  Init();

  Window window("Slot Machine", 1280, 720);
  window.resource_manager()->AddSearchPath(ExtractDirectory(argv[0]) +
                                           "/resources/");

  GameScene scene;
  window.PushScene(&scene);
  Run();

  Quit();
}