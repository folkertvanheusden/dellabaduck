#pragma once

#include <functional>


class Vertex
{
private:
	const int v, dim;

public:
	Vertex(const int v, const int dim);
	Vertex(const int x, const int y, const int dim);
	Vertex(const Vertex & v);
	virtual ~Vertex();

	bool operator<(const Vertex & rhs) const;
	bool operator==(const Vertex & rhs) const;
	bool operator()(const Vertex & a, const Vertex & b) const;

	struct HashFunction {
		size_t operator()(const Vertex & v) const {
			return std::hash<int>()(v.v);
		}
	};

	int getDim() const;
	int getV() const;
	int getX() const;
	int getY() const;
};
