#pragma once

#include <functional>


class Vertex
{
private:
	int  v     { 0     };
	int  dim   { 0     };
	bool valid { false };

public:
	Vertex(const int v, const int dim);
	Vertex(const int x, const int y, const int dim);
	Vertex(const Vertex & v);
	virtual ~Vertex();

	static Vertex from_str(const std::string & descr, const int dim);

	Vertex & operator=(const Vertex & in);
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
	Vertex left()  const;
	Vertex right() const;

	std::string to_str(const bool sgf = false) const;

	int getDim() const;
	int getV() const;
	int getX() const;
	int getY() const;
};
