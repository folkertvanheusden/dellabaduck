#include <cassert>

#include "vertex.h"


Vertex::Vertex(const int v, const int dim) : v(v), dim(dim), valid(v >= 0 && v < dim * dim)
{
}

Vertex::Vertex(const int x, const int y, const int dim) : v(y * dim + x), dim(dim), valid(x >= 0 && y >= 0 && x < dim && y < dim)
{
}

Vertex::Vertex(const Vertex & v) : v(v.getV()), dim(v.getDim()), valid(v.getV())
{
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

Vertex Vertex::left() const
{
	int x = v % dim;
	int y = v / dim;

	return { x - 1, y, dim };
}

Vertex Vertex::right() const
{
	int x = v % dim;
	int y = v / dim;

	return { x + 1, y, dim };
}
