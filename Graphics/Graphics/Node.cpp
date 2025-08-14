#pragma once
#include <vector>

using namespace std;
class Edge;
class Node
{
private:
    double x,y;
    int Color;
    double g; //distance from start point as sum of costs along the path (so far)
    Node* parent;
    vector<Edge*> outgoing;

public:
    
};