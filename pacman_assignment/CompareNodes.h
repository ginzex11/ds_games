#pragma once

#include "Node.h"

class CompareNodes
{
public:
    bool operator()(Node* pn1, Node* pn2)
    {
        return pn1->getF() > pn2->getF();  // For min-heap behavior in priority_queue
    }
};