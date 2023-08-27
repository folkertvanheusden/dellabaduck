#pragma once

#include <functional>


class Vertex
{
private:
	const int  v     { 0     };
	const int  dim   { 0     };
	const bool valid { false };

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

	bool isValid() const { return valid; }

	Vertex up()    const { return { v - dim, dim }; }
	Vertex down()  const { return { v + dim, dim }; }
	Vertex left()  const { return { v - 1,   dim }; }
	Vertex right() const { return { v + 1,   dim }; }

	int getDim() const;
	int getV() const;
	int getX() const;
	int getY() const;
};
