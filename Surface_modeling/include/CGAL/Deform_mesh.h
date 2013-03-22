// Copyright (c) 2011 GeometryFactory
// All rights reserved.
//
// This file is part of CGAL (www.cgal.org); you may redistribute it under
// the terms of the Q Public License version 1.0.
// See the file LICENSE.QPL distributed with CGAL.
//
// Licensees holding a valid commercial license may use this file in
// accordance with the commercial license agreement provided with the software.
//
// This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
// WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
//
// $URL:$
// $Id:$
//
// Author(s)     : Yin Xu, Andreas Fabri

#ifndef CGAL_DEFORM_MESH_H
#define CGAL_DEFORM_MESH_H

#include <CGAL/internal/Surface_modeling/Weights.h>
#include <CGAL/internal/Surface_modeling/Spokes_and_rims_iterator.h>

#include <CGAL/trace.h>
#include <CGAL/Timer.h>
#include <CGAL/boost/graph/graph_traits_Polyhedron_3.h>
#include <CGAL/boost/graph/properties_Polyhedron_3.h>
#include <CGAL/boost/graph/halfedge_graph_traits_Polyhedron_3.h>
#include <CGAL/FPU_extension.h>

#include <Eigen/Eigen>
#include <Eigen/SVD>

#include <set>
#include <vector>
#include <list>

#include "boost/tuple/tuple.hpp"

namespace CGAL {

/// \ingroup PkgSurfaceModeling,
///@brief Deformation algorithm type
enum Deformation_type
{ 
  ORIGINAL_ARAP,  /**< use original as-rigid-as possible algorithm */
  SPOKES_AND_RIMS /**< use spokes and rims version of as-rigid-as possible algorithm */
};

/**
 * \ingroup PkgSurfaceModeling
 * @brief Class providing the functionalities for deforming a triangulated surface mesh
 *
 * @tparam Polyhedron_ model of HalfedgeGraph
 * @tparam SparseLinearAlgebraTraitsWithPreFactor_d_ sparse linear solver for square sparse linear systems (link to concept?)
 * @tparam VertexIndexMap_ model of <a href="http://www.boost.org/doc/libs/release/libs/property_map/doc/ReadWritePropertyMap.html">`ReadWritePropertyMap`</a>  with Deform_mesh::vertex_descriptor as key and `unsigned int` as value type
 * @tparam EdgeIndexMap_ model of <a href="http://www.boost.org/doc/libs/release/libs/property_map/doc/ReadWritePropertyMap.html">`ReadWritePropertyMap`</a>  with Deform_mesh::edge_descriptor as key and `unsigned int` as value type
 * @tparam deformation_type non-type template parameter from ::Deformation_type for selecting deformation algorithm
 */
template <
  class Polyhedron_, 
  class SparseLinearAlgebraTraits_d_, 
  class VertexIndexMap_, 
  class EdgeIndexMap_,
  Deformation_type deformation_type = SPOKES_AND_RIMS
  >
class Deform_mesh
{
//Typedefs
public:

  /// \name Types from template parameters
  /// @{
  // typedefed template parameters, main reason is doxygen creates autolink to typedefs but not template parameters
  typedef Polyhedron_ Polyhedron; /**< model of HalfedgeGraph */
  typedef SparseLinearAlgebraTraits_d_ SparseLinearAlgebraTraits_d; /**< sparse linear solver for square sparse linear systems */
  typedef VertexIndexMap_ VertexIndexMap; /**< model of <a href="http://www.boost.org/doc/libs/release/libs/property_map/doc/ReadWritePropertyMap.html">`ReadWritePropertyMap`</a>  with Deform_mesh::vertex_descriptor as key and `unsigned int` as value type */
  typedef EdgeIndexMap_ EdgeIndexMap; /**< model of <a href="http://www.boost.org/doc/libs/release/libs/property_map/doc/ReadWritePropertyMap.html">`ReadWritePropertyMap`</a>  with Deform_mesh::edge_descriptor as key and `unsigned int` as value type */
  /// @}

  typedef typename boost::graph_traits<Polyhedron>::vertex_descriptor	vertex_descriptor; /**< The type for vertex representative objects */
  typedef typename boost::graph_traits<Polyhedron>::edge_descriptor		edge_descriptor;   /**< The type for edge representative objects */

  typedef typename Polyhedron::Traits::Vector_3  Vector; /**<The type for Vector_3 from Polyhedron traits */
  typedef typename Polyhedron::Traits::Point_3   Point;  /**<The type for Point_3 from Polyhedron traits */

private:
  typedef Deform_mesh<Polyhedron, SparseLinearAlgebraTraits_d, VertexIndexMap, EdgeIndexMap, deformation_type> Self;
  // Repeat Polyhedron types
  typedef typename boost::graph_traits<Polyhedron>::vertex_iterator     vertex_iterator;
  typedef typename boost::graph_traits<Polyhedron>::edge_iterator       edge_iterator;
  typedef typename boost::graph_traits<Polyhedron>::in_edge_iterator    in_edge_iterator;
  typedef typename boost::graph_traits<Polyhedron>::out_edge_iterator   out_edge_iterator;
  
  typedef internal::Spokes_and_rims_iterator<Polyhedron> Rims_iterator;

  // Handle container types
  typedef std::list<vertex_descriptor>  Handle_container;
  typedef std::list<Handle_container>   Handle_group_container;
public:
  /** The type for returned handle group representative from Deform_mesh::create_handle_group()*/
  typedef typename Handle_group_container::iterator       Handle_group;
  /** Const version of Handle_group*/
  typedef typename Handle_group_container::const_iterator Const_handle_group;
  /** The type for iterating over handles */
  typedef typename Handle_container::iterator             Handle_iterator;
   /** Const version of Handle_iterator*/
  typedef typename Handle_container::const_iterator       Const_handle_iterator;
// Data members.
public:
  Polyhedron& polyhedron;															/**< Source triangulated surface mesh for modeling */

private:
  std::vector<Point> original;                        // original positions of roi (size: ros + boundary_of_ros)
  std::vector<Point> solution;                        // storing position of ros vertices during iterations (size: ros + boundary_of_ros)

  VertexIndexMap vertex_index_map;                    // storing indices of ros vertices
  EdgeIndexMap   edge_index_map;                      // storing indices of ros related edges

  std::vector<vertex_descriptor> roi;                 // region of interest
  std::vector<vertex_descriptor> ros;                 // region of solution, including roi and hard constraints on boundary of roi
  std::vector<bool> is_roi;                           // (size: ros)
  std::vector<bool> is_hdl;                           // (size: ros)

  std::vector<double> edge_weight;                    // weight of edges only those who are incident to ros 
  std::vector<Eigen::Matrix3d> rot_mtr;               // rotation matrices of ros vertices (size: ros)

  SparseLinearAlgebraTraits_d m_solver;               // linear sparse solver
  unsigned int iterations;                            // number of maximal iterations
  double tolerance;                                   // tolerance of convergence 

  bool need_preprocess;                               // is there any need to call preprocess() function
  Handle_group_container handle_group_list;           // user specified handles

private:
  Deform_mesh(const Self& s) { } 

// Public methods
public:
/// \name Preprocess Section
/// @{
  /**
   * The constructor for deformation object
   *
   * @pre is there anyway to add @a polyhedron.is_pure_triangle() in halfedgegraph
   * @param polyhedron triangulated surface mesh for modeling
   * @param vertex_index_map vertex index map for associating ids with region of interest vertices
   * @param edge_index_map edge index map for associating ids with region of interest edges
   * @param iterations see explanations set_iterations(unsigned int iterations)
   * @param tolerance  see explanations set_tolerance(double tolerance)
   */
  Deform_mesh(Polyhedron& polyhedron, 
              VertexIndexMap vertex_index_map, 
              EdgeIndexMap edge_index_map,
              unsigned int iterations = 5,
              double tolerance = 1e-4)
    : polyhedron(polyhedron), vertex_index_map(vertex_index_map), edge_index_map(edge_index_map),
      iterations(iterations), tolerance(tolerance), need_preprocess(true)
  {
    // CGAL_precondition(polyhedron.is_pure_triangle());   
  }

  /**
   * Clear the internal state of the object, after cleanup the object can be treated as if it is just constructed
   */
  void clear()
  {
    need_preprocess = true;
    // clear vertices
    roi.clear();
    handle_group_list.clear();
    // no need to clear vertex index map (or edge) since they are going to be reassigned 
    // (at least the useful parts will be reassigned)
  }

  /**
   * Create a new empty handle group for inserting handles.
   * insert_handle(Handle_group handle_group, vertex_descriptor vd) or insert_handle(Handle_group handle_group, InputIterator begin, InputIterator end)
   * can be used for populating a group.
   * After inserting vertices, translate() or rotate() can be used for applying transformations on all vertices inside the group. 
   * @return created handle group representative (returned representative is valid until erase_handle(Handle_group handle_group) is called)
   */
  Handle_group create_handle_group()
  {
    // no need to need_preprocess = true;
    handle_group_list.push_back(Handle_container());
    return --handle_group_list.end();
  }
  
  /**
   * Insert the vertex into the handle group
   * @param handle_group group to be inserted into
   * @param vd vertex to be inserted
   */
  void insert_handle(Handle_group handle_group, vertex_descriptor vd)
  {
    need_preprocess = true;
    handle_group->push_back(vd);
  }

  /**
   * Insert vertices in the range to the handle group
   * @tparam InputIterator input iterator type which points to vertex descriptors
   * @param handle_group group to be inserted in
   * @param begin iterators specifying the range of vertices [begin, end) 
   * @param end iterators specifying the range of vertices [begin, end) 
   */
  template<class InputIterator>
  void insert_handle(Handle_group handle_group, InputIterator begin, InputIterator end)
  {
    need_preprocess = true;
    for( ;begin != end; ++begin)
    {
      insert_handle(handle_group, *begin);
    }
  }

  /**
   * Erase the handle group, and invalidate the representative so that it should not be used anymore.
   * @param handle_group group to be erased
   */
  void erase_handle(Handle_group handle_group)
  {
    need_preprocess = true;
    handle_group_list.erase(handle_group);
  }

  /**
   * Erase the vertex from the handle group, note that the handle group is not erased even if it becomes empty.
   * @param handle_group group to be erased from
   * @param vd vertex to be erased
   */
  void erase_handle(Handle_group handle_group, vertex_descriptor vd)
  {
    need_preprocess = true;
    typename Handle_container::iterator it 
      = std::find(handle_group->begin(), handle_group->end(), vd);
    if(it != handle_group->end())
    {
      handle_group->erase(it);
      // Although the handle group might get empty, we do not delete it from handle_group
    }
  }

  /** 
   * Return iterator [begin, end) for handle groups
   * @return tuple of [begin, end) as Handle_group
   * Note that the returned types are handle group representatives, so there is no need to use
   * dereference operator over iterators to reach representatives.
   */
  boost::tuple<Handle_group, Handle_group> handle_groups()
  {
    return boost::make_tuple(handle_group_list.begin(), handle_group_list.end());
  }

  /** 
   * Return iterator [begin, end) for handle groups
   * @return tuple of [begin, end) as Const_handle_group
   * Note that the returned types are handle group representatives, so there is no need to use
   * dereference operator over iterators to reach representatives.
   */
  boost::tuple<Const_handle_group, Const_handle_group> handle_groups() const
  {
    return boost::make_tuple(handle_group_list.begin(), handle_group_list.end());
  }

  /** 
   * Return iterator [begin, end) for handles inside the group
   * @param handle_group group containing the requested handles
   * @return tuple of [begin, end) as Handle_iterator
   * Use dereference operator to reach vertex_descriptors.
   */
  boost::tuple<Handle_iterator, Handle_iterator> handles(Handle_group handle_group)
  {
    return boost::make_tuple(handle_group->begin(), handle_group->end());
  }

  /** 
   * Return iterator [begin, end) for handles inside the group
   * @param handle_group group containing the requested handles
   * @return tuple of [begin, end) as Const_handle_iterator
   * Use dereference operator to reach vertex_descriptors.
   */
  boost::tuple<Const_handle_iterator, Const_handle_iterator> handles(Handle_group handle_group) const
  {
    return boost::make_tuple(handle_group->begin(), handle_group->end());
  }

  /** 
   * Return iterator [begin, end) for handles inside the group
   * @param handle_group group containing the requested handles
   * @return tuple of [begin, end) as Const_handle_iterator
   * Use dereference operator to reach vertex_descriptors.
   */
  boost::tuple<Const_handle_iterator, Const_handle_iterator> handles(Const_handle_group handle_group) const
  {
    return boost::make_tuple(handle_group->begin(), handle_group->end());
  }

  /**
   * Insert vertices in the range to region of interest
   * @tparam InputIterator input iterator type which points to vertex descriptors
   * @param begin iterators specifying the range of vertices [begin, end) 
   * @param end iterators specifying the range of vertices [begin, end) 
   */
  template<class InputIterator>
  void insert_roi(InputIterator begin, InputIterator end)
  {
    need_preprocess = true;
    for( ;begin != end; ++begin)
    {
      insert_roi(*begin);
    }
  }

  /**
   * Insert the vertex to region of interest
   * @param vd vertex to be inserted
   */
  void insert_roi(vertex_descriptor vd)   
  {
    need_preprocess = true;
    roi.push_back(vd);
  }

  /**
   * Erease the vertex from region of interest
   * @param vd vertex to be erased
   */
  void erase_roi(vertex_descriptor vd)   
  {
    need_preprocess = true;
    typename std::vector<vertex_descriptor>::iterator it = std::find(roi.begin(), roi.end(), vd);
    if(it != roi.end())
    {
      roi.erase(it);
    }
  }
  /**
   * Necessary precomputation work before beginning deformation.
   * It needs to be called after insertion of vertices as handles or roi is done.
   * For edge weights cotangent weights are used by default.
   * @return true if Laplacian matrix factorization is successful.
   * A common reason for failure is that the system is rank deficient, 
   * which happens if there is no path between a free vertex and a handle vertex (i.e. both fixed and user-inserted).
   * @see Deform_mesh::preprocess(WeightCalculator weight_calculator) for using custom weights
   */
  bool preprocess()
  {
    if(deformation_type == SPOKES_AND_RIMS) 
    {
      return preprocess(internal::Single_cotangent_weight<Polyhedron>());
    }
    else
    {
      return preprocess(internal::Cotangent_weight<Polyhedron>());
    }
  }
  /** 
   * see explanations in preprocess()
   * @tparam WeightCalculator model of SurfaceModelingWeightCalculator
   * @param weight_calculator function object or pointer for weight calculation
   * @return true if Laplacian matrix factorization is successful
   */
  template<class WeightCalculator>
  bool preprocess(WeightCalculator weight_calculator)
  {
    need_preprocess = false;

    region_of_solution();
    compute_edge_weight(weight_calculator); // compute_edge_weight() should be called after region_of_solution()

    // Assemble linear system A*X=B
    typename SparseLinearAlgebraTraits_d::Matrix A(ros.size()); // matrix is definite positive, and not necessarily symmetric
    assemble_laplacian(A);		

    // Pre-factorizing the linear system A*X=B
    double D;
    return m_solver.pre_factor(A, D);
  }
/// @} Preprocess Section

/// \name Utilities
/// @{
  /**
   * Set the number of iterations used in deform()
   */
  void set_iterations(unsigned int iterations)
  {
    this->iterations = iterations;
  }

  
   /// @brief Set the tolerance of convergence used in deform().
   /// Set to zero if energy based termination is not required, which also eliminates energy calculation effort in each iteration. 
   ///
   /// tolerance > \f$|energy(m_i) - energy(m_{i-1})| / energy(m_i)\f$ will be used as a termination criterium.
  void set_tolerance(double tolerance)
  {
    this->tolerance = tolerance;
  }
/// @} Utilities

/// \name Deform Section
/// @{  
  /**
   * Translate the handle group by translation,
   * in other words every handle vertex in the handle_group is translated from its original position
   * (i.e. position of the vertex at the time of the call Deform_mesh::preprocess()).
   * @param handle_group representative of the handle group which is subject to translation
   * @param translation translation vector 
   */
  void translate(Handle_group handle_group, const Vector& translation)
  {
    if(need_preprocess) { preprocess(); }

    for(typename Handle_container::iterator it = handle_group->begin();
      it != handle_group->end(); ++it)
    {
      std::size_t v_id = id(*it);
      solution[v_id] = original[v_id] + translation;
    }
  }

  /**
   * Rotate the handle group around rotation center by quaternion then translate it by translation 
   * from its original position (i.e. position of the vertex at the time of the call Deform_mesh::preprocess()).
   * @tparam Quaternion model of SurfaceModelingQuaternion
   * @tparam Vect model of SurfaceModelingVect
   * @param handle_group representative of the handle group which is subject to rotation
   * @param rotation_center center of rotation
   * @param quat rotation holder quaternion
   * @param translation post translation vector
   */
  template <typename Quaternion, typename Vect>
  void rotate(Handle_group handle_group, const Point& rotation_center, const Quaternion& quat, const Vect& translation)
  {
    if(need_preprocess) { preprocess(); }

    for(typename Handle_container::iterator it = handle_group->begin();
      it != handle_group->end(); ++it)
    {
      std::size_t v_id = id(*it);

      Point p = CGAL::ORIGIN + ( original[v_id] - rotation_center);
      Vect v = quat * Vect(p.x(),p.y(),p.z());
      p = Point(v[0], v[1], v[2]) + (rotation_center - CGAL::ORIGIN);
      p = p + Vector(translation[0],translation[1],translation[2]);

      solution[v_id] = p;
    }
  }

  /*
   * Rotate the handle group around center of original positions of handles in the group by quaternion then translate it by translation 
   * from its original position (i.e. position of the vertex at the time of the call Deform_mesh::preprocess()).
   * @tparam Quaternion model of SurfaceModelingQuaternion
   * @tparam Vect model of SurfaceModelingVect
   * @param handle_group representative of the handle group which is subject to rotation
   * @param quat rotation holder quaternion
   * @param translation post translation vector
   */

  //template <typename Quaternion, typename Vect>
  //void rotate(Handle_group handle_group, const Quaternion& quat, const Vect& translation)
  //{
  //  Point center_acc(0, 0, 0);
  //  for(typename Handle_container::iterator it = handle_group->begin();
  //    it != handle_group->end(); ++it)
  //  {
  //    center_acc = center_acc + (original[id(*it)] - CGAL::ORIGIN);
  //  }
  //  std::size_t handles_size = handle_group->size();
  //  Point center(center_acc.x() / handles_size, center_acc.y() / handles_size, center_acc.z() / handles_size);
  //  rotate(handle_group, center, quat, translation);
  //}

  /**
   * Assign the target position for the handle vertex 
   * @param vd handle vertex to be assigned target position
   * @param target_position constrained position
   */
  void assign(vertex_descriptor vd, const Point& target_position)
  {
    if(need_preprocess) { preprocess(); }

    solution[id(vd)] = target_position;
  }

  /**
   * Deformation on roi vertices.
   * @see set_iterations(unsigned int iterations), set_tolerance(double tolerance), deform(unsigned int iterations, double tolerance)
   */
  void deform()
  {
    deform(iterations, tolerance);
  }

  /**
   * Deformation on roi vertices.
   * Instant values for iterations and tolerance can be used as one-time parameters.
   * @param iterations number of iterations for optimization procedure
   * @param tolerance tolerance of convergence (see explanations set_tolerance(double tolerance))
   */
  void deform(unsigned int iterations, double tolerance)
  {
    // CGAL_precondition(!need_preprocess || !"preprocess() need to be called before deforming!");
    if(need_preprocess) { preprocess(); }

    // Note: no energy based termination occurs at first iteration
    // because comparing energy of original model (before deformation) and deformed model (deformed_1_iteration)
    // simply does not make sense, comparison is meaningful between deformed_(i)_iteration & deformed_(i+1)_iteration

    double energy_this = 0; // initial value is not important, because we skip first iteration
    double energy_last;

    // iterations
    for ( unsigned int ite = 0; ite < iterations; ++ite)
    {
      // main steps of optimization
      update_solution();       
      optimal_rotations_svd(); 

      // energy based termination
      if(tolerance > 0.0 && (ite + 1) < iterations) // if tolerance <= 0 then don't compute energy 
      {                                             // also no need compute energy if this iteration is the last iteration
        energy_last = energy_this;
        energy_this = energy();
        if(energy_this < 0)
        {
           std::cout << "Negative energy" << std::endl;
        }

        if(ite != 0) // skip first iteration
        {
          double energy_dif = std::abs((energy_last - energy_this) / energy_this);
          if ( energy_dif < tolerance ) { break; }
        }
      }
    }
    // copy solution to target mesh
    assign_solution();
  }
/// @} Deform Section

private:

  /// Compute cotangent weights of all edges
  template<class WeightCalculator>
  void compute_edge_weight(WeightCalculator weight_calculator)
  {
    if(deformation_type == SPOKES_AND_RIMS) 
    {
      compute_edge_weight_spokes_and_rims(weight_calculator);
    }
    else
    {
      compute_edge_weight_arap(weight_calculator);
    }
  }
  template<class WeightCalculator>
  void compute_edge_weight_arap(WeightCalculator weight_calculator)
  {
    std::set<edge_descriptor> have_id; // edges which has assigned ids (and also weights are calculated)

    // iterate over ros vertices and calculate weights for edges which are incident to ros
    std::size_t next_edge_id = 0;
    for (std::size_t i = 0; i < ros.size(); i++)
    {
      vertex_descriptor vi = ros[i];
      in_edge_iterator e, e_end;
      for (boost::tie(e,e_end) = boost::in_edges(vi, polyhedron); e != e_end; e++)
      {
        if(have_id.insert(*e).second) 
        { // we haven't assigned an id yet, also no weights calculated
          put(edge_index_map, *e, next_edge_id++);

          double wij = weight_calculator(*e, polyhedron); // edge(pi - pj)
          edge_weight.push_back(wij);
        }
      }// end of edge loop
    }// end of ros loop
  }
  template<class WeightCalculator>
  void compute_edge_weight_spokes_and_rims(WeightCalculator weight_calculator)
  {
    std::set<edge_descriptor> have_id; // edges which has assigned ids (and also weights are calculated)

    // iterate over ros vertices and calculate weights for edges which are incident to ros
    std::size_t next_edge_id = 0;
    for (std::size_t i = 0; i < ros.size(); i++)
    {       
      vertex_descriptor vi = ros[i];
      out_edge_iterator e_begin, e_end;
      boost::tie(e_begin, e_end) = boost::out_edges(vi, polyhedron);

      for (Rims_iterator rims_it(e_begin, polyhedron); rims_it.get_iterator() != e_end; ++rims_it )
      {
        edge_descriptor active_edge = rims_it.get_descriptor();

        if(have_id.insert(active_edge).second) 
        {  // we haven't assigned an id yet, also no weights calculated
          put(edge_index_map, active_edge, next_edge_id++);
          double wji = weight_calculator(active_edge, polyhedron); // edge(pj - pi)
          edge_weight.push_back(wji);

          edge_descriptor opp = CGAL::opposite_edge(active_edge, polyhedron);

          put(edge_index_map, opp, next_edge_id++);
          have_id.insert(opp);
          double wij = weight_calculator(opp, polyhedron); // edge(pi - pj)
          edge_weight.push_back(wij);
        }
      }// end of edge loop
    }// end of ros loop
  }

  /// Assigns id to one rign neighbor of vd, and also push them into push_vector
  void assign_id_to_one_ring(vertex_descriptor vd, 
                             std::size_t& next_id, 
                             std::vector<vertex_descriptor>& push_vector,
                             std::set<vertex_descriptor>& have_id)
  {
    in_edge_iterator e, e_end;
    for (boost::tie(e,e_end) = boost::in_edges(vd, polyhedron); e != e_end; e++)
    {
      vertex_descriptor vt = boost::source(*e, polyhedron);
      if(have_id.insert(vt).second)  // neighboring vertex which is outside of roi and not visited previously (i.e. need an id)
      {
        put(vertex_index_map, vt, next_id++);
        push_vector.push_back(vt);        
      }
    }
  }
  
  /// Find region of solution, including roi and hard constraints, which is the 1-ring vertices out roi
  void region_of_solution()
  {
    ros.clear(); // clear ros
    
    ros.insert(ros.end(), roi.begin(), roi.end()); 

    ////////////////////////////////////////////////////////////////
    // assign id to vertices inside: roi, boundary of roi (roi + boundary of roi = ros),
    //                               and boundary of ros
    // keep in mind that id order does not have to be compatible with ros order

    std::set<vertex_descriptor> have_id;         // keep vertices which are assigned an id
    
    for(std::size_t i = 0; i < roi.size(); i++)  // assign id to all roi vertices
    {
      put(vertex_index_map, roi[i], i);
    }

    have_id.insert(roi.begin(), roi.end());      // mark roi vertices since they have ids now

    // now assign an id to vertices on boundary of roi
    std::size_t next_ros_index = roi.size();
    for(std::size_t i = 0; i < roi.size(); i++)
    {
      assign_id_to_one_ring(roi[i], next_ros_index, ros, have_id);
    }

    std::vector<vertex_descriptor> outside_ros;
    // boundary of ros also must have ids because in SVD calculation,
    // one-ring neighbor of ROS vertices are reached. 
    for(std::size_t i = roi.size(); i < ros.size(); i++)
    {
      assign_id_to_one_ring(ros[i], next_ros_index, outside_ros, have_id);
    }
    ////////////////////////////////////////////////////////////////

    // initialize the rotation matrices (size: ros)
    rot_mtr.resize(ros.size());
    for(std::size_t i = 0; i < rot_mtr.size(); i++)
    {
      rot_mtr[i].setIdentity();
    }
    
    // initialize solution and original (size: ros + boundary_of_ros)

    // for simplifying coding effort, I also put boundary of ros into solution and original
    // because boundary of ros vertices are reached in optimal_rotations_svd() and energy()
    solution.resize(ros.size() + outside_ros.size());
    original.resize(ros.size() + outside_ros.size());
    
    for(std::size_t i = 0; i < ros.size(); i++)
    {
      std::size_t v_id = id(ros[i]);
      solution[v_id] = ros[i]->point();
      original[v_id] = ros[i]->point();
    }
    for(std::size_t i = 0; i < outside_ros.size(); ++i)
    {
      std::size_t v_id = id(outside_ros[i]);
      original[v_id] = outside_ros[i]->point();
      solution[v_id] = outside_ros[i]->point();
    }

    // initialize flag vectors of roi, handle, ros 
    is_roi.assign(ros.size(), false);
    is_hdl.assign(ros.size(), false);
    for(std::size_t i = 0; i < roi.size(); i++)
    {
      std::size_t v_id = id(roi[i]);
      is_roi[v_id] = true;
    }
    for(typename Handle_group_container::iterator it_group = handle_group_list.begin(); 
      it_group != handle_group_list.end(); ++it_group)
    {
      for(typename Handle_container::iterator it_vertex = it_group->begin(); 
        it_vertex != it_group->end(); ++it_vertex)
      {
        std::size_t v_id = id(*it_vertex);
        is_hdl[v_id] = true;
      }
    }
  }

  /// Assemble Laplacian matrix A of linear system A*X=B
  void assemble_laplacian(typename SparseLinearAlgebraTraits_d::Matrix& A)
  {
    if(deformation_type == SPOKES_AND_RIMS) 
    {
      assemble_laplacian_spokes_and_rims(A);
    }
    else
    {
      assemble_laplacian_arap(A);
    }
  }
  /// Construct matrix that corresponds to left-hand side of eq:lap_ber in user manual
  /// Also constraints are integrated as eq:lap_energy_system in user manual
  void assemble_laplacian_arap(typename SparseLinearAlgebraTraits_d::Matrix& A)
  {
    /// assign cotangent Laplacian to ros vertices
    for(std::size_t k = 0; k < ros.size(); k++)
    {
      vertex_descriptor vi = ros[k];
      std::size_t vi_id = id(vi);
      if ( is_roi[vi_id] && !is_hdl[vi_id] )          // vertices of ( roi - hdl )
      {
        double diagonal = 0;
        in_edge_iterator e, e_end;
        for (boost::tie(e,e_end) = boost::in_edges(vi, polyhedron); e != e_end; e++)
        {
          vertex_descriptor vj = boost::source(*e, polyhedron);
          double wij = edge_weight[id(*e)];  // edge(pi - pj)
          double wji = edge_weight[id(CGAL::opposite_edge(*e, polyhedron))]; // edge(pi - pj)
          double total_weight = wij + wji;

          A.set_coef(vi_id, id(vj), -total_weight, true);	// off-diagonal coefficient
          diagonal += total_weight;  
        }
        // diagonal coefficient
        A.set_coef(vi_id, vi_id, diagonal, true);
      }
      else
      {
        A.set_coef(vi_id, vi_id, 1.0, true);
      }
    }
  }
  /// Construct matrix that corresponds to left-hand side of eq:lap_ber_rims in user manual
  /// Also constraints are integrated as eq:lap_energy_system in user manual
  void assemble_laplacian_spokes_and_rims(typename SparseLinearAlgebraTraits_d::Matrix& A)
  {
    /// assign cotangent Laplacian to ros vertices    
    for(std::size_t k = 0; k < ros.size(); k++)
    {
      vertex_descriptor vi = ros[k];
      std::size_t vi_id = id(vi);
      if ( is_roi[vi_id] && !is_hdl[vi_id] ) // vertices of ( roi - hdl ): free vertices
      {
        double diagonal = 0;
        out_edge_iterator e, e_end;
        for (boost::tie(e,e_end) = boost::out_edges(vi, polyhedron); e != e_end; e++)
        {
          double total_weight = 0;
          // an edge contribute to energy only if it is part of an incident triangle 
          // (i.e it should not be a border edge)
          if(!boost::get(CGAL::edge_is_border, polyhedron, *e)) 
          {
            double wji = edge_weight[id(*e)]; // edge(pj - pi)
            total_weight += wji; 
          }

          edge_descriptor opp = CGAL::opposite_edge(*e, polyhedron);
          if(!boost::get(CGAL::edge_is_border, polyhedron, opp))
          {
            double wij = edge_weight[id(opp)]; // edge(pi - pj)
            total_weight += wij;
          }

          // place coefficient to matrix
          vertex_descriptor vj = boost::target(*e, polyhedron);
          A.set_coef(vi_id, id(vj), -total_weight, true);	// off-diagonal coefficient
          diagonal += total_weight; 
        }
        // diagonal coefficient
        A.set_coef(vi_id, vi_id, diagonal, true);
      }
      else // constrained vertex
      {
        A.set_coef(vi_id, vi_id, 1.0, true); 
      }
    }
  }

  /// Local step of iterations, computing optimal rotation matrices using SVD decomposition, stable
  void optimal_rotations_svd()
  {
    if(deformation_type == SPOKES_AND_RIMS) 
    {
      optimal_rotations_svd_spokes_and_rims();
    }
    else
    {
      optimal_rotations_svd_arap();
    }
  }
  void optimal_rotations_svd_arap()
  {
    Eigen::Matrix3d cov;            // covariance matrix
    Eigen::JacobiSVD<Eigen::Matrix3d> svd;       // SVD solver         

    // only accumulate ros vertices
    for ( std::size_t k = 0; k < ros.size(); k++ )
    {
      vertex_descriptor vi = ros[k];
      std::size_t vi_id = id(vi);
      // compute covariance matrix (user manual eq:cov_matrix)
      cov.setZero();

      in_edge_iterator e, e_end;
      for (boost::tie(e,e_end) = boost::in_edges(vi, polyhedron); e != e_end; e++)
      {
        vertex_descriptor vj = boost::source(*e, polyhedron);
        std::size_t vj_id = id(vj);

        Eigen::Vector3d    pij = sub_to_col(original[vi_id], original[vj_id]);
        Eigen::RowVector3d qij = sub_to_row(solution[vi_id], solution[vj_id]);
        double wij = edge_weight[id(*e)];

        cov += wij * (pij * qij);
      }
      // svd decomposition
      svd.compute( cov, Eigen::ComputeFullU | Eigen::ComputeFullV );
      const Eigen::Matrix3d& u = svd.matrixU();
      const Eigen::Matrix3d& v = svd.matrixV();

      // extract rotation matrix
      rot_mtr[vi_id] = v*u.transpose();

      // checking negative determinant of r
      if ( rot_mtr[vi_id].determinant() < 0 )    // changing the sign of column corresponding to smallest singular value
      {
        Eigen::Matrix3d u_m = u;
        u_m.col(2) *= -1;      // singular values are always sorted in decresing order so use column 2
        rot_mtr[vi_id] = v*u_m.transpose(); // re-extract rotation matrix
      }
    }
  }
  void optimal_rotations_svd_spokes_and_rims()
  {    
    Eigen::Matrix3d cov;            // covariance matrix
    Eigen::JacobiSVD<Eigen::Matrix3d> svd;       // SVD solver         

    // only accumulate ros vertices
    for ( std::size_t k = 0; k < ros.size(); k++ )
    {
      vertex_descriptor vi = ros[k];
      std::size_t vi_id = id(vi);
      // compute covariance matrix
      cov.setZero();
      //iterate through all triangles 
      out_edge_iterator e, e_end;
      for (boost::tie(e,e_end) = boost::out_edges(vi, polyhedron); e != e_end; e++)
      {
        if(boost::get(CGAL::edge_is_border, polyhedron, *e)) { continue; } // no facet 
        // iterate edges around facet
        edge_descriptor edge_around_facet = *e;
        do
        {
          vertex_descriptor v1 = boost::target(edge_around_facet, polyhedron);
          vertex_descriptor v2 = boost::source(edge_around_facet, polyhedron);

          std::size_t v1_id = id(v1); std::size_t v2_id = id(v2);
        
          Eigen::Vector3d    p12 = sub_to_col(original[v1_id], original[v2_id]);
          Eigen::RowVector3d q12 = sub_to_row(solution[v1_id], solution[v2_id]);

          double w12 = edge_weight[id(edge_around_facet)];

          cov += w12 * (p12 * q12);
        } while( (edge_around_facet = CGAL::next_edge(edge_around_facet, polyhedron)) != *e);
      }
  
      // svd decomposition
      svd.compute( cov, Eigen::ComputeFullU | Eigen::ComputeFullV );
      const Eigen::Matrix3d& u = svd.matrixU();
      const Eigen::Matrix3d& v = svd.matrixV();

      // extract rotation matrix
      rot_mtr[vi_id] = v*u.transpose();

      // checking negative determinant of r
      if ( rot_mtr[vi_id].determinant() < 0 )    // changing the sign of column corresponding to smallest singular value
      {
        Eigen::Matrix3d u_m = u;
        u_m.col(2) *= -1;      // singular values are always sorted in decresing order so use column 2
        rot_mtr[vi_id] = v*u_m.transpose(); // re-extract rotation matrix
      }
    }
  }

  /// Global step of iterations, updating solution
  void update_solution()
  {
    if(deformation_type == SPOKES_AND_RIMS) 
    {
      update_solution_spokes_and_rims();
    }
    else
    {
      update_solution_arap();
    }
  }
  // calculate right-hand side of eq:lap_ber in user manual and solve the system
  void update_solution_arap()
  {
    typename SparseLinearAlgebraTraits_d::Vector X(ros.size()), Bx(ros.size());
    typename SparseLinearAlgebraTraits_d::Vector Y(ros.size()), By(ros.size());
    typename SparseLinearAlgebraTraits_d::Vector Z(ros.size()), Bz(ros.size());

    // assemble right columns of linear system
    for ( std::size_t k = 0; k < ros.size(); k++ )
    {
      vertex_descriptor vi = ros[k];
      std::size_t vi_id = id(vi);

      if ( is_roi[vi_id] && !is_hdl[vi_id] ) 
      {// free vertices
        Eigen::Vector3d xyz; xyz.setZero(); // sum of right-hand side of eq:lap_ber in user manual

        in_edge_iterator e, e_end;
        for (boost::tie(e,e_end) = boost::in_edges(vi, polyhedron); e != e_end; e++)
        {
          vertex_descriptor vj = boost::source(*e, polyhedron);
          std::size_t vj_id = id(vj); 
          
          Eigen::Vector3d pij = sub_to_col(original[vi_id], original[vj_id]);
          double wij = edge_weight[id(*e)];
          double wji = edge_weight[id(CGAL::opposite_edge(*e, polyhedron))];

          xyz += (wij*rot_mtr[vi_id] + wji*rot_mtr[vj_id]) * pij; 
        }
        Bx[vi_id] = xyz(0,0); By[vi_id] = xyz(1,0); Bz[vi_id] = xyz(2,0);
      }
      else 
      {// constrained vertex
        Bx[vi_id] = solution[vi_id].x(); By[vi_id] = solution[vi_id].y(); Bz[vi_id] = solution[vi_id].z();
      }
    }

    // solve "A*X = B".
    m_solver.linear_solver(Bx, X); m_solver.linear_solver(By, Y); m_solver.linear_solver(Bz, Z);

    // copy to solution
    for (std::size_t i = 0; i < ros.size(); i++)
    {
      std::size_t v_id = id(ros[i]);
      Point p(X[v_id], Y[v_id], Z[v_id]);
      solution[v_id] = p;
    }
  }
  // calculate right-hand side of eq:lap_ber_rims in user manual and solve the system
  void update_solution_spokes_and_rims()
  {
    typename SparseLinearAlgebraTraits_d::Vector X(ros.size()), Bx(ros.size());
    typename SparseLinearAlgebraTraits_d::Vector Y(ros.size()), By(ros.size());
    typename SparseLinearAlgebraTraits_d::Vector Z(ros.size()), Bz(ros.size());

    // assemble right columns of linear system
    for ( std::size_t k = 0; k < ros.size(); k++ )
    {
      vertex_descriptor vi = ros[k];
      std::size_t vi_id = id(vi);

      if ( is_roi[vi_id] && !is_hdl[vi_id] ) 
      {// free vertices
        Eigen::Vector3d xyz;  xyz.setZero(); // sum of right-hand side of eq:lap_ber_rims in user manual

        out_edge_iterator e, e_end;
        for (boost::tie(e,e_end) = boost::out_edges(vi, polyhedron); e != e_end; e++)
        {
          vertex_descriptor vj = boost::target(*e, polyhedron);
          std::size_t vj_id = id(vj); 

          Eigen::Vector3d pij = sub_to_col(original[vi_id], original[vj_id]);
          
          if(!boost::get(CGAL::edge_is_border, polyhedron, *e))
          {
            vertex_descriptor vn = boost::target(CGAL::next_edge(*e, polyhedron), polyhedron); // opp vertex of e_ij
            double wji = edge_weight[id(*e)] / 3.0;  // edge(pj - pi)
            xyz += wji*(rot_mtr[vi_id] + rot_mtr[vj_id] + rot_mtr[id(vn)])*pij;
          }

          edge_descriptor opp = CGAL::opposite_edge(*e, polyhedron);
          if(!boost::get(CGAL::edge_is_border, polyhedron, opp))
          {
            vertex_descriptor vm = boost::target(CGAL::next_edge(opp, polyhedron), polyhedron); // other opp vertex of e_ij
            double wij = edge_weight[id(opp)] / 3.0;  // edge(pi - pj)
            xyz += wij*(rot_mtr[vi_id] + rot_mtr[vj_id] + rot_mtr[id(vm)])*pij;
          }
        }
        Bx[vi_id] = xyz(0,0); By[vi_id] = xyz(1,0); Bz[vi_id] = xyz(2,0); 
      }
      else 
      {// constrained vertices
        Bx[vi_id] = solution[vi_id].x(); By[vi_id] = solution[vi_id].y(); Bz[vi_id] = solution[vi_id].z();
      }
    }
    // solve "A*X = B".
    m_solver.linear_solver(Bx, X); m_solver.linear_solver(By, Y); m_solver.linear_solver(Bz, Z);

    // copy to solution
    for (std::size_t i = 0; i < ros.size(); i++)
    {
      std::size_t v_id = id(ros[i]);
      Point p(X[v_id], Y[v_id], Z[v_id]);
      solution[v_id] = p;
    }
  }

  /// Assign solution to target mesh
  void assign_solution()
  {
    for(std::size_t i = 0; i < ros.size(); ++i){
      std::size_t v_id = id(ros[i]);
      if(is_roi[v_id])
      {
        ros[i]->point() = solution[v_id];
      }
    }
  }

  /// Compute modeling energy
  double energy() const 
  {
    if(deformation_type == SPOKES_AND_RIMS) 
    {
      return energy_spokes_and_rims();
    }
    else
    {
      return energy_arap();
    }
  }
  double energy_arap() const
  {
    double sum_of_energy = 0;
    // only accumulate ros vertices
    for( std::size_t k = 0; k < ros.size(); k++ )
    {
      vertex_descriptor vi = ros[k];
      std::size_t vi_id = id(vi);

      in_edge_iterator e, e_end;
      for (boost::tie(e,e_end) = boost::in_edges(vi, polyhedron); e != e_end; e++)
      {
        vertex_descriptor vj = boost::source(*e, polyhedron);
        std::size_t vj_id = id(vj);

        Eigen::Vector3d pij = sub_to_col(original[vi_id], original[vj_id]);
        Eigen::Vector3d qij = sub_to_col(solution[vi_id], solution[vj_id]);
        double wij = edge_weight[id(*e)];

        sum_of_energy += wij * (qij - rot_mtr[vi_id]*pij).squaredNorm();
      }
    }
    return sum_of_energy;
  }
  double energy_spokes_and_rims() const
  {
    double sum_of_energy = 0;
    // only accumulate ros vertices
    for( std::size_t k = 0; k < ros.size(); k++ )
    {
      vertex_descriptor vi = ros[k];
      std::size_t vi_id = id(vi);
      //iterate through all triangles 
      out_edge_iterator e, e_end;
      for (boost::tie(e,e_end) = boost::out_edges(vi, polyhedron); e != e_end; e++)
      {
        if(boost::get(CGAL::edge_is_border, polyhedron, *e)) { continue; } // no facet 
        // iterate edges around facet
        edge_descriptor edge_around_facet = *e;
        do
        {
          vertex_descriptor v1 = boost::target(edge_around_facet, polyhedron);
          vertex_descriptor v2 = boost::source(edge_around_facet, polyhedron);
          std::size_t v1_id = id(v1); std::size_t v2_id = id(v2);

          Eigen::Vector3d p12 = sub_to_col(original[v1_id], original[v2_id]);
          Eigen::Vector3d q12 = sub_to_col(solution[v1_id], solution[v2_id]);
          double w12 = edge_weight[id(edge_around_facet)];
         
          sum_of_energy += w12 * (q12 - rot_mtr[vi_id]*p12).squaredNorm();

        } while( (edge_around_facet = CGAL::next_edge(edge_around_facet, polyhedron)) != *e);
      }
    }
    return sum_of_energy;
  }

  /// p1 - p2, return Eigen Column Vector
  Eigen::Vector3d sub_to_col(const Point& p1, const Point& p2) const
  {
    return Eigen::Vector3d(p1.x() - p2.x(), p1.y() - p2.y(), p1.z() - p2.z());
  }
  /// p1 - p2, return Eigen Row Vector
  Eigen::RowVector3d sub_to_row(const Point& p1, const Point& p2) const
  {
    return Eigen::RowVector3d(p1.x() - p2.x(), p1.y() - p2.y(), p1.z() - p2.z());
  }
  /// shorthand of get(vertex_index_map, v)
  std::size_t id(vertex_descriptor v) const
  {
    return get(vertex_index_map, v);
  }
  /// shorthand of get(edge_index_map, e)
  std::size_t id(edge_descriptor e) const
  {
    return get(edge_index_map, e);
  }

  #ifdef CGAL_DEFORM_EXPERIMENTAL      // Experimental stuff, needs further testing

  double norm_1(const Eigen::Matrix3d& X)
  {
    double sum = 0;
    for ( int i = 0; i < 3; i++ )
    {
      for ( int j = 0; j < 3; j++ )
      {
        sum += abs(X(i,j));
      }
    }
    return sum;
  }

  double norm_inf(const Eigen::Matrix3d& X)
  {
    double max_abs = abs(X(0,0));
    for ( int i = 0; i < 3; i++ )
    {
      for ( int j = 0; j < 3; j++ )
      {
        double new_abs = abs(X(i,j));
        if ( new_abs > max_abs )
        {
          max_abs = new_abs;
        }
      }
    }
    return max_abs;
  }

  // polar decomposition using Newton's method, with warm start, stable but slow
  // not used, need to be investigated later
  void polar_newton(const Eigen::Matrix3d& A, Eigen::Matrix3d &U, double tole)
  {
    Eigen::Matrix3d X = A;
    Eigen::Matrix3d Y;
    double alpha, beta, gamma;
    do 
    {
      Y = X.inverse();
      alpha = sqrt( norm_1(X) * norm_inf(X) );
      beta = sqrt( norm_1(Y) * norm_inf(Y) );
      gamma = sqrt(beta/alpha);
      X = 0.5*( gamma*X + Y.transpose()/gamma );

    } while ( abs(gamma-1) > tole );

    U = X;
  }
  
  // polar decomposition using Eigen, 5 times faster than SVD
  template<typename Mat>
  void polar_eigen(const Mat& A, Mat& R, bool& SVD)
  {
    typedef typename Mat::Scalar Scalar;
    typedef Eigen::Matrix<typename Mat::Scalar,3,1> Vec;

    const Scalar th = std::sqrt(Eigen::NumTraits<Scalar>::dummy_precision());

    Eigen::SelfAdjointEigenSolver<Mat> eig;
    feclearexcept(FE_UNDERFLOW);
    eig.computeDirect(A.transpose()*A);
    if(fetestexcept(FE_UNDERFLOW) || eig.eigenvalues()(0)/eig.eigenvalues()(2)<th)
    {
      // The computation of the eigenvalues might have diverged.
      // Fallback to an accurate SVD based decomposiiton method.
      Eigen::JacobiSVD<Mat> svd;
      svd.compute(A, Eigen::ComputeFullU | Eigen::ComputeFullV );
      const Mat& u = svd.matrixU(); const Mat& v = svd.matrixV();
      R = u*v.transpose();
      SVD = true;
      return;
    }

    Vec S = eig.eigenvalues().cwiseSqrt();
    R = A  * eig.eigenvectors() * S.asDiagonal().inverse()
      * eig.eigenvectors().transpose();
    SVD = false;

    if(std::abs(R.squaredNorm()-3.) > th)
    {
      // The computation of the eigenvalues might have diverged.
      // Fallback to an accurate SVD based decomposiiton method.
      Eigen::JacobiSVD<Mat> svd;
      svd.compute(A, Eigen::ComputeFullU | Eigen::ComputeFullV );
      const Mat& u = svd.matrixU(); const Mat& v = svd.matrixV();
      R = u*v.transpose();
      SVD = true;
      return;
    }
  }

  // Local step of iterations, computing optimal rotation matrices using Polar decomposition
  void optimal_rotations_polar()
  {
    Eigen::Matrix3d u, v;           // orthogonal matrices 
    Eigen::Vector3d w;              // singular values
    Eigen::Matrix3d cov;            // covariance matrix
    Eigen::Matrix3d r;
    Eigen::JacobiSVD<Eigen::Matrix3d> svd;      // SVD solver, for non-positive covariance matrices
    int num_svd = 0;
    bool SVD = false;

    // only accumulate ros vertices
    for ( std::size_t i = 0; i < ros.size(); i++ )
    {
      vertex_descriptor vi = ros[i];
      // compute covariance matrix
      cov.setZero();

      in_edge_iterator e, e_end;
      for (boost::tie(e,e_end) = boost::in_edges(vi, polyhedron); e != e_end; e++)
      {
        vertex_descriptor vj = boost::source(*e, polyhedron);
        Vector pij = original[get(vertex_index_map, vi)] - original[get(vertex_index_map, vj)];
        Vector qij = solution[get(vertex_index_map, vi)] - solution[get(vertex_index_map, vj)];
        double wij = edge_weight[get(edge_index_map, *e)];
        for (int j = 0; j < 3; j++)
        {
          for (int k = 0; k < 3; k++)
          {
            cov(j, k) += wij*pij[j]*qij[k]; 
          }
        }
      }

      // svd decomposition
      if (cov.determinant() > 0)
      {
        polar_eigen<Eigen::Matrix3d> (cov, r, SVD);
        //polar_newton(cov, r, 1e-4);   
        if(SVD)
          num_svd++;
        r.transposeInPlace();     // the optimal rotation matrix should be transpose of decomposition result
      }
      else
      {
        svd.compute( cov, Eigen::ComputeFullU | Eigen::ComputeFullV );
        u = svd.matrixU(); v = svd.matrixV(); w = svd.singularValues();
        r = v*u.transpose();
        num_svd++;
      }
      
      // checking negative determinant of covariance matrix
      if ( r.determinant() < 0 )    // back to SVD method
      {
        if (cov.determinant() > 0)
        {
          svd.compute( cov, Eigen::ComputeFullU | Eigen::ComputeFullV );
          u = svd.matrixU(); v = svd.matrixV(); w = svd.singularValues();
          num_svd++;
        }
        for (int j = 0; j < 3; j++)
        {
          int j0 = j;
          int j1 = (j+1)%3;
          int j2 = (j1+1)%3;
          if ( w[j0] <= w[j1] && w[j0] <= w[j2] )    // smallest singular value as j0
          {
            u(0, j0) = - u(0, j0);
            u(1, j0) = - u(1, j0);
            u(2, j0) = - u(2, j0);
            break;
          }
        }

        // re-extract rotation matrix
        r = v*u.transpose();
      }

      rot_mtr[i] = r;
    }

    double svd_percent = (double)(num_svd)/ros.size();
    CGAL_TRACE_STREAM << svd_percent*100 << "% percentage SVD decompositions;";
    CGAL_TRACE_STREAM << num_svd << " SVD decompositions\n";

  }

#endif
};
} //namespace CGAL
#endif  // CGAL_DEFORM_MESH_H

