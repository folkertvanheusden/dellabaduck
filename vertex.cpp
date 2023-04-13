#include <assert.h>

#include "vertex.h"


Vertex::Vertex(const int v, const int dim) : v(v), dim(dim)
{
       assert(dim & 1);
}

Vertex::Vertex(const int x, const int y, const int dim) : v(y * dim + x), dim(dim)
{
       assert(dim & 1);
}

Vertex::Vertex(const Vertex & v) : v(v.getV()), dim(v.getDim())
{
       assert(dim & 1);
}

Vertex::~Vertex()
{
}

bool Vertex::operator<(const Vertex & rhs) const
{
       return v < rhs.v;
}

bool Vertex::operator==(const Vertex & rhs) const
{
       return v == rhs.v;
}

bool Vertex::operator()(const Vertex & a, const Vertex & b) const
{
       return a.v > b.v;
}

int Vertex::getDim() const
{
       return dim;
}

int Vertex::getV() const
{
       return v;
}

int Vertex::getX() const
{
       return v % dim;
}

int Vertex::getY() const
{
       return v / dim;
}
