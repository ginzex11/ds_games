#pragma once

#include "Node.h"

class CompareNodes
{
public:
    bool operator()(Node* pn1, Node* pn2)
    {
        return pn1->getG() > pn2->getG();  // Changed to > for min-heap behavior
    }
};