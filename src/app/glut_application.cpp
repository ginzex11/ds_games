#include "smart/app/glut_application.hpp"

#include <array>
#include <cstdlib>
#include <sstream>

#if defined(__has_include)
#  if __has_include(<GL/freeglut.h>)
#    include <GL/freeglut.h>
#  elif __has_include(<GL/glut.h>)
#    include <GL/glut.h>
#  else
#    error "No GLUT header available."
#  endif
#else
#  include <GL/glut.h>
#endif

#if defined(__has_include)
#  if __has_include(<GL/glu.h>)
#    include <GL/glu.h>
#  endif
#else
#  include <GL/glu.h>
#endif

namespace smart::app {
namespace {
constexpr int TIMER_INTERVAL_MS = 150;
std::array<float, 3> team_color(TeamId id)
{
    if (id == TeamId::Blue) {
        return {0.2f, 0.6f, 0.95f};
    }
    return {0.95f, 0.45f, 0.1f};
}

std::array<float, 3> terrain_color(world::TerrainType type)
{
    switch (type) {
    case world::TerrainType::Empty:
        return {0.12f, 0.12f, 0.16f};
    case world::TerrainType::Rock:
        return {0.25f, 0.25f, 0.27f};
    case world::TerrainType::Tree:
        return {0.05f, 0.25f, 0.05f};
    case world::TerrainType::Water:
        return {0.05f, 0.15f, 0.35f};
    case world::TerrainType::AmmoDepot:
        return {0.7f, 0.7f, 0.15f};
    case world::TerrainType::MedicalDepot:
        return {0.9f, 0.75f, 0.2f};
    }
    return {0.2f, 0.2f, 0.2f};
}

void draw_rect(float x, float y, float size, const std::array<float, 3> &color)
{
    glColor3f(color[0], color[1], color[2]);
    glBegin(GL_QUADS);
    glVertex2f(x, y);
    glVertex2f(x + size, y);
    glVertex2f(x + size, y + size);
    glVertex2f(x, y + size);
    glEnd();
}

void draw_outline(float x, float y, float size, const std::array<float, 3> &color)
{
    glColor3f(color[0], color[1], color[2]);
    glBegin(GL_LINE_LOOP);
    glVertex2f(x, y);
    glVertex2f(x + size, y);
    glVertex2f(x + size, y + size);
    glVertex2f(x, y + size);
    glEnd();
}

} // namespace

GlutApplication *GlutApplication::instance_ = nullptr;

GlutApplication::GlutApplication(int argc, char **argv)
{
    instance_ = this;

    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);

    simulation_ = std::make_unique<Simulation>(20, 30);
    const auto &map = simulation_->map();
    window_width_ = margin_ * 2 + map.cols() * cell_size_;
    window_height_ = margin_ * 2 + map.rows() * cell_size_ + 80;

    glutInitWindowSize(window_width_, window_height_);
    glutCreateWindow("Smart Movement Tactics");

    configure_gl();

    glutDisplayFunc(&GlutApplication::display_callback);
    glutReshapeFunc(&GlutApplication::reshape_callback);
    glutKeyboardFunc(&GlutApplication::keyboard_callback);
    glutTimerFunc(TIMER_INTERVAL_MS, &GlutApplication::timer_callback, 0);
}

int GlutApplication::run()
{
    glutMainLoop();
    return 0;
}

void GlutApplication::configure_gl()
{
    glClearColor(0.05f, 0.05f, 0.07f, 1.0f);
    glShadeModel(GL_FLAT);
    glDisable(GL_DEPTH_TEST);
}

void GlutApplication::update_viewport(int width, int height)
{
    window_width_ = width;
    window_height_ = height;
    glViewport(0, 0, width, height);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(0.0, static_cast<GLdouble>(width), static_cast<GLdouble>(height), 0.0);
}

void GlutApplication::draw_scene() const
{
    draw_map();
    if (show_paths_) {
        draw_agent_paths();
    }
    draw_agents();
    if (show_visibility_) {
        draw_visibility_overlay();
    }
    draw_hud();
}

void GlutApplication::draw_map() const
{
    const auto &map = simulation_->map();
    for (int r = 0; r < map.rows(); ++r) {
        for (int c = 0; c < map.cols(); ++c) {
            const auto cell = geometry::Position{r, c};
            const auto world_pos_x = static_cast<float>(margin_ + c * cell_size_);
            const auto world_pos_y = static_cast<float>(margin_ + r * cell_size_);
            draw_rect(world_pos_x, world_pos_y, static_cast<float>(cell_size_), terrain_color(map.at(cell)));
        }
    }

    glColor3f(0.08f, 0.08f, 0.1f);
    glBegin(GL_LINES);
    for (int r = 0; r <= map.rows(); ++r) {
        const float y = static_cast<float>(margin_ + r * cell_size_);
        glVertex2f(static_cast<float>(margin_), y);
        glVertex2f(static_cast<float>(margin_ + map.cols() * cell_size_), y);
    }
    for (int c = 0; c <= map.cols(); ++c) {
        const float x = static_cast<float>(margin_ + c * cell_size_);
        glVertex2f(x, static_cast<float>(margin_));
        glVertex2f(x, static_cast<float>(margin_ + map.rows() * cell_size_));
    }
    glEnd();
}

void GlutApplication::draw_agents() const
{
    const auto &agents = simulation_->all_agents();
    for (const auto &agent : agents) {
        if (!agent.is_alive()) {
            continue;
        }
        const auto color = team_color(agent.team);
        const float x = static_cast<float>(margin_ + agent.position.col * cell_size_ + 4);
        const float y = static_cast<float>(margin_ + agent.position.row * cell_size_ + 4);
        draw_rect(x, y, static_cast<float>(cell_size_ - 8), color);

        glColor3f(0.0f, 0.0f, 0.0f);
        glRasterPos2f(x + cell_size_ / 2.0f - 4.0f, y + cell_size_ / 2.0f + 4.0f);
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, agent.symbol());
    }
}

void GlutApplication::draw_agent_paths() const
{
    const auto &map = simulation_->map();
    const auto &agents = simulation_->all_agents();
    glLineWidth(2.0f);
    for (const auto &agent : agents) {
        if (agent.path.empty()) {
            continue;
        }
        const auto color = team_color(agent.team);
        glColor3f(color[0], color[1], color[2]);
        glBegin(GL_LINE_STRIP);
        for (const auto &tile : agent.path) {
            const float x = static_cast<float>(margin_ + tile.col * cell_size_ + cell_size_ / 2);
            const float y = static_cast<float>(margin_ + tile.row * cell_size_ + cell_size_ / 2);
            glVertex2f(x, y);
        }
        glEnd();
    }
    (void)map;
}

void GlutApplication::draw_visibility_overlay() const
{
    const auto grid = simulation_->commander_visibility(TeamId::Blue);
    glColor4f(0.2f, 0.6f, 1.0f, 0.15f);
    glBegin(GL_QUADS);
    for (int r = 0; r < grid.rows(); ++r) {
        for (int c = 0; c < grid.cols(); ++c) {
            if (!grid.contains({r, c})) {
                continue;
            }
            const float x = static_cast<float>(margin_ + c * cell_size_);
            const float y = static_cast<float>(margin_ + r * cell_size_);
            glVertex2f(x, y);
            glVertex2f(x + cell_size_, y);
            glVertex2f(x + cell_size_, y + cell_size_);
            glVertex2f(x, y + cell_size_);
        }
    }
    glEnd();
}

void GlutApplication::draw_hud() const
{
    std::ostringstream oss;
    oss << "[Space] Pause  [N] Step  [R] Reset  [V] Toggle Visibility  [P] Toggle Paths";
    if (simulation_->paused()) {
        oss << "  |  PAUSED";
    }

    glColor3f(0.85f, 0.85f, 0.85f);
    glRasterPos2f(static_cast<float>(margin_), static_cast<float>(window_height_ - margin_ / 2));
    const auto hud = oss.str();
    for (char ch : hud) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, ch);
    }
}

void GlutApplication::update_simulation()
{
    simulation_->update();
}

void GlutApplication::display_callback()
{
    glClear(GL_COLOR_BUFFER_BIT);
    instance_->draw_scene();
    glutSwapBuffers();
}

void GlutApplication::reshape_callback(int width, int height)
{
    instance_->update_viewport(width, height);
}

void GlutApplication::keyboard_callback(unsigned char key, int, int)
{
    switch (key) {
    case 27: // escape
        std::exit(0);
    case ' ':
        instance_->simulation_->toggle_pause();
        break;
    case 'n':
    case 'N':
        instance_->simulation_->step_once();
        break;
    case 'r':
    case 'R':
        instance_->simulation_->reset();
        break;
    case 'v':
    case 'V':
        instance_->show_visibility_ = !instance_->show_visibility_;
        break;
    case 'p':
    case 'P':
        instance_->show_paths_ = !instance_->show_paths_;
        break;
    default:
        break;
    }
    glutPostRedisplay();
}

void GlutApplication::timer_callback(int)
{
    if (!instance_->simulation_->paused()) {
        instance_->update_simulation();
    }
    glutPostRedisplay();
    glutTimerFunc(TIMER_INTERVAL_MS, &GlutApplication::timer_callback, 0);
}

} // namespace smart::app
