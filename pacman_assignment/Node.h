#ifndef NODE_H
#define NODE_H

#include "Cell.h"

class Node {
private:
    Cell* cell;
    double g; // cost from start
    double h; // heuristic cost to goal
    double f; // total cost (g + h)
    Node* parent;

public:
    // Default constructor
    Node() : cell(nullptr), g(0.0), h(0.0), f(0.0), parent(nullptr) {}
    
    Node(Cell* c, double gCost = 0.0, double hCost = 0.0, Node* p = nullptr) 
        : cell(c), g(gCost), h(hCost), f(gCost + hCost), parent(p) {}
    
    Cell* getCell() const { return cell; }
    double getG() const { return g; }
    double getH() const { return h; }
    double getF() const { return f; }
    Node* getParent() const { return parent; }
    
    void setG(double gCost) { 
        g = gCost; 
        f = g + h; 
    }
    
    void setH(double hCost) { 
        h = hCost; 
        f = g + h; 
    }
    
    void setParent(Node* p) { parent = p; }
};

#endif