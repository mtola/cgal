// Copyright (c) 2015  Geometry Factory
// All rights reserved.
//
// This file is part of CGAL (www.cgal.org)
//
// $URL$
// $Id$
// SPDX-License-Identifier: LGPL-3.0-or-later OR LicenseRef-Commercial
//
// Author(s) : Simon Giraudot

#ifndef CGAL_IO_PLY_H
#define CGAL_IO_PLY_H

#include <CGAL/IO/PLY/PLY_reader.h>
#include <CGAL/IO/PLY/PLY_writer.h>

#include <CGAL/property_map.h>

namespace CGAL {

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
/// Read
//HEdgesRange" = range of std::pair<unsigned int, unsigned int>
//HUVRange = range of std::pair<float, float>
template <class PointRange, class PolygonRange, class ColorRange, class HEdgesRange, class HUVRange>
bool read_PLY(std::istream& is,
              PointRange& points,
              PolygonRange& polygons,
              HEdgesRange& hedges,
              ColorRange& fcolors,
              ColorRange& vcolors,
              HUVRange& huvs,
              bool /* verbose */ = false)
{
  typedef typename PointRange::value_type Point_3;
  typedef typename ColorRange::value_type Color_rgb;
  if(!is.good())
  {
    std::cerr << "Error: cannot open file" << std::endl;
    return false;
  }

  IO::internal::PLY_reader reader;

  if(!(reader.init(is)))
  {
    is.setstate(std::ios::failbit);
    return false;
  }

  for(std::size_t i = 0; i < reader.number_of_elements(); ++ i)
  {
    IO::internal::PLY_element& element = reader.element(i);

    if(element.name() == "vertex" || element.name() == "vertices")
    {
      bool has_colors = false;
      std::string rtag = "r", gtag = "g", btag = "b";
      if((element.has_property<boost::uint8_t>("red") || element.has_property<boost::uint8_t>("r")) &&
         (element.has_property<boost::uint8_t>("green") || element.has_property<boost::uint8_t>("g")) &&
         (element.has_property<boost::uint8_t>("blue") || element.has_property<boost::uint8_t>("b")))
      {
        has_colors = true;
        if(element.has_property<boost::uint8_t>("red"))
        {
          rtag = "red"; gtag = "green"; btag = "blue";
        }
      }

      for(std::size_t j = 0; j < element.number_of_items(); ++ j)
      {
        for(std::size_t k = 0; k < element.number_of_properties(); ++ k)
        {
          IO::internal::PLY_read_number* property = element.property(k);
          property->get(is);

          if(is.fail())
            return false;
        }

        std::tuple<Point_3, boost::uint8_t, boost::uint8_t, boost::uint8_t> new_vertex;

        if(has_colors)
        {
          IO::internal::process_properties(element, new_vertex,
                                            make_ply_point_reader(CGAL::make_nth_of_tuple_property_map<0>(new_vertex)),
                                            std::make_pair(CGAL::make_nth_of_tuple_property_map<1>(new_vertex),
                                                           PLY_property<boost::uint8_t>(rtag.c_str())),
                                            std::make_pair(CGAL::make_nth_of_tuple_property_map<2>(new_vertex),
                                                           PLY_property<boost::uint8_t>(gtag.c_str())),
                                            std::make_pair(CGAL::make_nth_of_tuple_property_map<3>(new_vertex),
                                                           PLY_property<boost::uint8_t>(btag.c_str())));

          vcolors.push_back(Color_rgb(get<1>(new_vertex), get<2>(new_vertex), get<3>(new_vertex)));
        }
        else
          IO::internal::process_properties(element, new_vertex,
                                            make_ply_point_reader(CGAL::make_nth_of_tuple_property_map<0>(new_vertex)));

        points.push_back(get<0>(new_vertex));
      }
    }
    else if(element.name() == "face" || element.name() == "faces")
    {
      if(element.has_property<std::vector<boost::int32_t> >("vertex_indices"))
        IO::internal::read_PLY_faces<boost::int32_t>(is, element, polygons, fcolors, "vertex_indices");
      else if(element.has_property<std::vector<boost::uint32_t> >("vertex_indices"))
        IO::internal::read_PLY_faces<boost::uint32_t>(is, element, polygons, fcolors, "vertex_indices");
      else if(element.has_property<std::vector<boost::int32_t> >("vertex_index"))
        IO::internal::read_PLY_faces<boost::int32_t>(is, element, polygons, fcolors, "vertex_index");
      else if(element.has_property<std::vector<boost::uint32_t> >("vertex_index"))
        IO::internal::read_PLY_faces<boost::uint32_t>(is, element, polygons, fcolors, "vertex_index");
      else
      {
        std::cerr << "Error: can't find vertex indices in PLY input" << std::endl;
        return false;
      }
    }
    else if(element.name() == "halfedge" )
    {
      bool has_uv = false;
      std::string stag = "source", ttag = "target", utag = "u", vtag = "v";
      if( element.has_property<unsigned int>("source") &&
          element.has_property<unsigned int>("target") &&
          element.has_property<float>("u") &&
          element.has_property<float>("v"))
      {
        has_uv = true;
      }

      cpp11::tuple<unsigned int, unsigned int, float, float, float>  new_hedge;
      for(std::size_t j = 0; j < element.number_of_items(); ++ j)
      {
        for(std::size_t k = 0; k < element.number_of_properties(); ++ k)
        {
          IO::internal::PLY_read_number* property = element.property(k);
          property->get(is);

          if(is.fail())
            return false;
        }

        if(has_uv)
        {
          IO::internal::process_properties(element, new_hedge,
                                            std::make_pair(CGAL::make_nth_of_tuple_property_map<0>(new_hedge),
                                                           PLY_property<unsigned int>(stag.c_str())),
                                            std::make_pair(CGAL::make_nth_of_tuple_property_map<1>(new_hedge),
                                                           PLY_property<unsigned int>(ttag.c_str())),
                                            std::make_pair(CGAL::make_nth_of_tuple_property_map<2>(new_hedge),
                                                           PLY_property<float>(utag.c_str())),
                                            std::make_pair(CGAL::make_nth_of_tuple_property_map<3>(new_hedge),
                                                           PLY_property<float>(vtag.c_str())));
          hedges.push_back(std::make_pair(get<0>(new_hedge), get<1>(new_hedge)));
          huvs.push_back(std::make_pair(get<2>(new_hedge), get<3>(new_hedge)));
        }
        else
        {
          IO::internal::process_properties(element, new_hedge,
                                            std::make_pair(CGAL::make_nth_of_tuple_property_map<0>(new_hedge),
                                                           PLY_property<unsigned int>(stag.c_str())),
                                            std::make_pair(CGAL::make_nth_of_tuple_property_map<1>(new_hedge),
                                                           PLY_property<unsigned int>(ttag.c_str()))
                                            );
        }
      }
    }
    else // Read other elements and ignore
    {
      for(std::size_t j = 0; j < element.number_of_items(); ++ j)
      {
        for(std::size_t k = 0; k < element.number_of_properties(); ++ k)
        {
          IO::internal::PLY_read_number* property = element.property(k);
          property->get(is);
          if(is.fail())
            return false;
        }
      }
    }
  }
  return !is.fail();
}

template <class PointRange, class PolygonRange, class ColorRange>
bool read_PLY(std::istream& is,
              PointRange& points,
              PolygonRange& polygons,
              ColorRange& fcolors,
              ColorRange& vcolors,
              bool /* verbose */ = false)
{

  std::vector<std::pair<unsigned int, unsigned int> > dummy_pui;
  std::vector<std::pair<float, float> > dummy_pf;

  return read_PLY(is, points, polygons, dummy_pui, fcolors, vcolors, dummy_pf);
}

/*!
 * \ingroup IOstreamFunctions
 *
 * reads the content of `is` into `points` and `polygons`, in the PLY format.
 *
 * `PointRange` is a model of the concepts `RandomAccessContainer`
 * and `BackInsertionSequence` whose `value type` is a `CGAL::Point_3`.
 *
 * `PolygonRange` is a model of the concepts `RandomAccessContainer`
 * and `BackInsertionSequence` whose `value type` is `std::size_t`.
 *
 * \see \ref IOStreamPLY
 */
template <class PointRange, class PolygonRange>
bool
read_PLY(std::istream& is,
         PointRange& points,
         PolygonRange& polygons,
         bool /* verbose */ = false)
{
  typedef typename PointRange::value_type Point_3;
  if(!is.good())
  {
    std::cerr << "Error: cannot open file" << std::endl;
    return false;
  }

  IO::internal::PLY_reader reader;

  if(!(reader.init(is)))
  {
    is.setstate(std::ios::failbit);
    return false;
  }

  for(std::size_t i = 0; i < reader.number_of_elements(); ++ i)
  {
    IO::internal::PLY_element& element = reader.element(i);

    if(element.name() == "vertex" || element.name() == "vertices")
    {
      for(std::size_t j = 0; j < element.number_of_items(); ++ j)
      {
        for(std::size_t k = 0; k < element.number_of_properties(); ++ k)
        {
          IO::internal::PLY_read_number* property = element.property(k);
          property->get(is);

          if(is.fail())
            return false;
        }

        Point_3 new_vertex;

        IO::internal::process_properties(element, new_vertex,
                                          make_ply_point_reader(CGAL::Identity_property_map<Point_3>()));

        points.push_back(new_vertex);
      }
    }
    else if(element.name() == "face" || element.name() == "faces")
    {
      std::vector<CGAL::Color> dummy;

      if(element.has_property<std::vector<boost::int32_t> >("vertex_indices"))
        IO::internal::read_PLY_faces<boost::int32_t>(is, element, polygons, dummy, "vertex_indices");
      else if(element.has_property<std::vector<boost::uint32_t> >("vertex_indices"))
        IO::internal::read_PLY_faces<boost::uint32_t>(is, element, polygons, dummy, "vertex_indices");
      else if(element.has_property<std::vector<boost::int32_t> >("vertex_index"))
        IO::internal::read_PLY_faces<boost::int32_t>(is, element, polygons, dummy, "vertex_index");
      else if(element.has_property<std::vector<boost::uint32_t> >("vertex_index"))
        IO::internal::read_PLY_faces<boost::uint32_t>(is, element, polygons, dummy, "vertex_index");
      else
      {
        std::cerr << "Error: can't find vertex indices in PLY input" << std::endl;
        return false;
      }
    }
    else // Read other elements and ignore
    {
      for(std::size_t j = 0; j < element.number_of_items(); ++ j)
      {
        for(std::size_t k = 0; k < element.number_of_properties(); ++ k)
        {
          IO::internal::PLY_read_number* property = element.property(k);
          property->get(is);

          if(is.fail())
            return false;
        }
      }
    }
  }

  return !is.fail();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
/// Write

/*!
 * \ingroup IOstreamFunctions
 *
 * writes the content of `points` and `polygons` in `out`, in the OFF format.
 *
 * `PointRange` is a model of the concepts `RandomAccessContainer`
 * and `BackInsertionSequence` whose `value type` is a `CGAL::Point_3`.
 *
 * `PolygonRange` is a model of the concepts `RandomAccessContainer`
 * and `BackInsertionSequence` whose `value type` is `std::size_t`.
 * \see \ref IOStreamOFF
 */
template <class PointRange, class PolygonRange>
bool write_PLY(std::ostream& out,
               const PointRange& points,
               const PolygonRange& polygons)
{

  typedef typename PointRange::value_type Point_3;
  typedef typename PolygonRange::value_type Polygon_3;
  if(!out.good())
  {
    std::cerr << "Error: cannot open file" << std::endl;
    return false;
  }

  // Write header
  out << "ply" << std::endl
      << ((get_mode(out) == IO::BINARY) ? "format binary_little_endian 1.0" : "format ascii 1.0") << std::endl
      << "comment Generated by the CGAL library" << std::endl
      << "element vertex " << points.size() << std::endl;

  IO::internal::output_property_header(out, make_ply_point_writer (CGAL::Identity_property_map<Point_3>()));

  out << "element face " << polygons.size() << std::endl;

  IO::internal::output_property_header(out, std::make_pair(CGAL::Identity_property_map<Polygon_3>(),
                                                            PLY_property<std::vector<int> >("vertex_indices")));

  out << "end_header" << std::endl;

  for (std::size_t i = 0; i < points.size(); ++ i)
    IO::internal::output_properties(out, points.begin() + i,
                                     make_ply_point_writer (CGAL::Identity_property_map<Point_3>()));

  for (std::size_t i = 0; i < polygons.size(); ++ i)
    IO::internal::output_properties(out, polygons.begin() + i,
                                     std::make_pair(CGAL::Identity_property_map<Polygon_3>(),
                                                    PLY_property<std::vector<int> >("vertex_indices")));

  return out.good();
}

} // namespace CGAL

#endif // CGAL_IO_PLY_H
