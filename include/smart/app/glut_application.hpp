#pragma once

#include "smart/simulation.hpp"

#include <memory>

namespace smart::app {

class GlutApplication {
public:
    GlutApplication(int argc, char **argv);
    int run();

private:
    std::unique_ptr<Simulation> simulation_;

    bool show_visibility_{false};
    bool show_paths_{true};

    int cell_size_{28};
    int margin_{32};

    int window_width_{0};
    int window_height_{0};

    void configure_gl();
    void update_viewport(int width, int height);
    void draw_scene() const;
    void draw_map() const;
    void draw_agents() const;
    void draw_agent_paths() const;
    void draw_visibility_overlay() const;
    void draw_hud() const;

    void update_simulation();

    static GlutApplication *instance_;

    static void display_callback();
    static void reshape_callback(int width, int height);
    static void keyboard_callback(unsigned char key, int, int);
    static void timer_callback(int value);
};

} // namespace smart::app
