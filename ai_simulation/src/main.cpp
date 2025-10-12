#include "simulation.h"
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
    
    switch (key) {
        case ' ':  // Space - toggle pause
            globalSimulation->togglePause();
            break;
        case 'r':  // R - reset
        case 'R':
            globalSimulation->reset();
            break;
        case '+':  // Speed up
        case '=':
            globalSimulation->speedUp();
            break;
        case '-':  // Slow down
        case '_':
            globalSimulation->slowDown();
            break;
        case 27:   // ESC - exit
            exit(0);
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
    
    // Initialize GLUT
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    glutInitWindowSize(WINDOW_WIDTH, WINDOW_HEIGHT);
    glutInitWindowPosition(100, 100);
    glutCreateWindow("AI Simulation Game - Team Combat");
    
    std::cout << "Window created successfully.\n";
    
    // Initialize OpenGL
    initializeOpenGL();
    
    std::cout << "OpenGL initialized.\n";
    
    // Create simulation
    globalSimulation = new Simulation();
    
    std::cout << "Simulation created.\n";
    std::cout << "\nControls:\n";
    std::cout << "  SPACE - Pause/Resume\n";
    std::cout << "  R     - Reset simulation\n";
    std::cout << "  +/-   - Speed up/slow down\n";
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
    
    // Register callbacks
    glutDisplayFunc(displayCallback);
    glutKeyboardFunc(keyboardCallback);
    glutTimerFunc(0, timerCallback, 0);
    
    // Start main loop
    lastFrameTime = std::chrono::high_resolution_clock::now();
    glutMainLoop();
    
    // Cleanup
    delete globalSimulation;
    
    return 0;
}
