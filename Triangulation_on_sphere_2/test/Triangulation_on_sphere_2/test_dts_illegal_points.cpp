#include <CGAL/Exact_predicates_inexact_constructions_kernel.h>

#include <CGAL/Delaunay_triangulation_sphere_traits_2.h>
#include <CGAL/Delaunay_triangulation_sphere_2.h>
#include <CGAL/Projection_sphere_traits_3.h>
#include <CGAL/Triangulation_sphere_2.h>

#include <iostream>
#include <vector>

typedef CGAL::Exact_predicates_inexact_constructions_kernel         K;
typedef CGAL::Delaunay_triangulation_sphere_traits_2<K>             Gt;
typedef CGAL::Projection_sphere_traits_3<K>                         Pgt;
typedef CGAL::Delaunay_triangulation_sphere_2<Gt>                   DTOS;
typedef CGAL::Delaunay_triangulation_sphere_2<Pgt>                  PDTOS;

void testProjection()
{
  typedef K::Point_3                                                Point;

  Pgt traits(Point(0,0,0));
  PDTOS pdtos(traits);

  Pgt::Construct_projected_point_3 cst = traits.construct_projected_point_3_object();

  double radius = 1000;
  pdtos.set_radius(radius);

  std::vector<Point> points;

  // legal points
  Point p1( radius/sqrt(2),               0,  radius/sqrt(2));
  Point p2(-radius/sqrt(3),  radius/sqrt(3), -radius/sqrt(3));
  Point p3(         radius,               0,               0);
  Point p4(-radius/sqrt(2), -radius/sqrt(2),               0);

  points.push_back(p1);
  points.push_back(p2);
  points.push_back(p3);
  points.push_back(p4);

  // points original not on sphere
  Point p5(radius, 0, -0.5*radius);
  points.push_back(p5);

  // Points too close
  Point p6(         5*radius, std::pow(2, -25),                                   0);
  Point p7(-7*radius/sqrt(3), 7*radius/sqrt(3), -7*radius/sqrt(3) + std::pow(2,-35));

  points.push_back(p6);
  points.push_back(p7);

  pdtos.insert(boost::make_transform_iterator(points.begin(), cst),
               boost::make_transform_iterator(points.end(), cst));

  assert(pdtos.number_of_vertices() == 5);
  assert(pdtos.is_valid());
}
void testDelaunay()
{
  typedef DTOS::Point                                   Point;

  const double radius =1000;
  DTOS dtos;
  dtos.set_radius(radius);

  std::vector<Point> points;
  // legal points
  Point p1( radius/sqrt(2),               0,  radius/sqrt(2));
  Point p2(-radius/sqrt(3),  radius/sqrt(3), -radius/sqrt(3));
  Point p3(         radius,               0,               0);
  Point p4(-radius/sqrt(2), -radius/sqrt(2),               0);

  points.push_back(p1);
  points.push_back(p2);
  points.push_back(p3);
  points.push_back(p4);

  // points not on sphere
  Point p5(-1.5*radius, 0,           0);
  Point p6(     radius, 0, -0.5*radius);
  points.push_back(p5);
  points.push_back(p6);

  // points to close
  Point p7(radius/sqrt(2), radius*std::pow(2, -15),          radius/sqrt(2));
  Point p8(        radius,                       0, radius*std::pow(2, -14));

  points.push_back(p7);
  points.push_back(p8);

  dtos.insert(points.begin(), points.end());
  dtos.is_valid();
  assert(dtos.number_of_vertices() == 4);

  dtos.clear();
  dtos.set_radius(radius);

  dtos.insert(p1);
  assert(dtos.number_of_vertices() == 1);
  dtos.insert(p1);
  assert(dtos.number_of_vertices() == 1);

  dtos.insert(p2);
  assert(dtos.number_of_vertices() == 2);
  dtos.insert(p2);
  assert(dtos.number_of_vertices() == 2);

  dtos.insert(p3);
  assert(dtos.number_of_vertices() == 3);
  dtos.insert(p3);
  assert(dtos.number_of_vertices() == 3);

  dtos.insert(p4);
  assert(dtos.number_of_vertices() == 4);
  dtos.insert(p4);
  assert(dtos.number_of_vertices() == 4);

  dtos.insert(p5);
  assert(dtos.number_of_vertices() == 4);
  dtos.insert(p6);
  assert(dtos.number_of_vertices() == 4);

  dtos.insert(p7);
  assert(dtos.number_of_vertices() == 4);

  dtos.insert(p8);
  assert(dtos.number_of_vertices() == 4);
  dtos.insert(p8);
  assert(dtos.number_of_vertices() == 4);
}

int main()
{
  std::cout << "Test inserting illegal points with Delaunay_traits" << std::endl;
  testDelaunay();

  std::cout << "Test inserting illegal points with Projection_traits" << std::endl;
  testProjection();

  return EXIT_SUCCESS;
}
