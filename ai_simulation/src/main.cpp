#include "simulation.h"
#include "Logger.h"
#include <GL/freeglut.h>
#include <chrono>

// Global simulation instance
Simulation* globalSimulation = nullptr;
auto lastFrameTime = std::chrono::high_resolution_clock::now();

/**
 * @brief Display callback for OpenGL
 */
void displayCallback() {
    if (globalSimulation) {
        globalSimulation->render();
    }
}

/**
 * @brief Timer callback for game updates
 */
void timerCallback(int value) {
    if (globalSimulation) {
        auto currentTime = std::chrono::high_resolution_clock::now();
        std::chrono::duration<float> deltaTime = currentTime - lastFrameTime;
        lastFrameTime = currentTime;
        
        globalSimulation->update(deltaTime.count());
        
        glutPostRedisplay();
    }
    
    // Call timer again (60 FPS)
    glutTimerFunc(16, timerCallback, 0);
}

/**
 * @brief Keyboard callback for controls
 */
void keyboardCallback(unsigned char key, int x, int y) {
    if (!globalSimulation) return;
    
    // Log key press to control log
    Logger::log(Logger::LogType::CONTROL, "Key pressed: " + std::to_string((int)key) + " ('" + std::string(1, key) + "')");
    
    switch (key) {
        case ' ':  // Space - toggle pause
            Logger::log(Logger::LogType::CONTROL, "Toggling pause");
            globalSimulation->togglePause();
            break;
        case 'r':  // R - reset
        case 'R':
            Logger::log(Logger::LogType::CONTROL, "Resetting simulation");
            globalSimulation->reset();
            break;
        case '+':  // Speed up
        case '=':
            Logger::log(Logger::LogType::CONTROL, "Speeding up");
            globalSimulation->speedUp();
            break;
        case '-':  // Slow down
        case '_':
            Logger::log(Logger::LogType::CONTROL, "Slowing down");
            globalSimulation->slowDown();
            break;
        case 'f':  // F - toggle fog of war
        case 'F':
            Logger::log(Logger::LogType::CONTROL, "Toggling fog of war");
            globalSimulation->toggleFogOfWar();
            break;
        case 'v':  // V - toggle vision cones
        case 'V':
            Logger::log(Logger::LogType::CONTROL, "Toggling vision cones");
            globalSimulation->toggleVisionCones();
            break;
        case 'w':  // W - toggle weapon ranges
        case 'W':
            Logger::log(Logger::LogType::CONTROL, "Toggling weapon ranges");
            globalSimulation->toggleWeaponRanges();
            break;
        case 27:   // ESC - exit
            Logger::log(Logger::LogType::CONTROL, "Exiting application");
            Logger::shutdown();
            exit(0);
            break;
        default:
            Logger::log(Logger::LogType::CONTROL, "Unhandled key");
            break;
    }
    
    glutPostRedisplay();
}

/**
 * @brief Initialize OpenGL settings
 */
void initializeOpenGL() {
    // Set clear color (background)
    glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
    
    // Set up orthographic projection for 2D rendering
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(0, WINDOW_WIDTH, 0, WINDOW_HEIGHT);
    
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    
    // Enable smooth shading
    glShadeModel(GL_SMOOTH);
    
    // Disable depth test for 2D
    glDisable(GL_DEPTH_TEST);
}

/**
 * @brief Main entry point
 */
int main(int argc, char** argv) {
    std::cout << "=================================================\n";
    std::cout << "  AI Simulation Game - Team Combat Strategy\n";
    std::cout << "=================================================\n\n";
    std::cout << "Initializing simulation...\n";
    
    // Initialize logger (logs will be in ai_simulation/logs/)
    Logger::initialize("logs");
    Logger::log(Logger::LogType::CONTROL, "=== Simulation Starting ===");
    
    // Initialize GLUT
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    glutInitWindowSize(WINDOW_WIDTH, WINDOW_HEIGHT);
    glutInitWindowPosition(100, 100);
    glutCreateWindow("AI Simulation Game - Team Combat");
    
    std::cout << "Window created successfully.\n";
    Logger::log(Logger::LogType::CONTROL, "Window created successfully");
    
    // Initialize OpenGL
    initializeOpenGL();
    
    std::cout << "OpenGL initialized.\n";
    Logger::log(Logger::LogType::CONTROL, "OpenGL initialized");
    
    // Create simulation
    globalSimulation = new Simulation();
    
    std::cout << "Simulation created.\n";
    Logger::log(Logger::LogType::CONTROL, "Simulation created");
    
    std::cout << "\nControls:\n";
    std::cout << "  SPACE - Pause/Resume\n";
    std::cout << "  R     - Reset simulation\n";
    std::cout << "  +/-   - Speed up/slow down\n";
    std::cout << "  F     - Toggle fog of war\n";
    std::cout << "  V     - Toggle vision cones\n";
    std::cout << "  W     - Toggle weapon ranges\n";
    std::cout << "  ESC   - Exit\n\n";
    std::cout << "Game Rules:\n";
    std::cout << "  - Blue team (left) vs Orange team (right)\n";
    std::cout << "  - Each team: 1 Commander (C), 2 Warriors (W), 1 Medic (M), 1 Supplier (P)\n";
    std::cout << "  - Warriors fight with limited ammo\n";
    std::cout << "  - Medics heal injured warriors\n";
    std::cout << "  - Suppliers resupply ammo\n";
    std::cout << "  - Commander coordinates team strategy\n";
    std::cout << "  - Eliminate all enemies to win!\n\n";
    std::cout << "Starting simulation...\n\n";
    
    Logger::log(Logger::LogType::CONTROL, "Starting main game loop");
    
    // Register callbacks
    glutDisplayFunc(displayCallback);
    glutKeyboardFunc(keyboardCallback);
    glutTimerFunc(0, timerCallback, 0);
    
    // Start main loop
    lastFrameTime = std::chrono::high_resolution_clock::now();
    glutMainLoop();
    
    // Cleanup
    Logger::log(Logger::LogType::CONTROL, "=== Simulation Ending ===");
    Logger::shutdown();
    delete globalSimulation;
    
    return 0;
}
