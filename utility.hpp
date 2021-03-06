#ifndef UTILITY_CPP
#define UTILITY_CPP

#include <boost/graph/astar_search.hpp>
#include <boost/graph/filtered_graph.hpp>
#include <boost/graph/grid_graph.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_int.hpp>
#include <boost/random/variate_generator.hpp>
#include <boost/unordered_map.hpp>
#include <boost/unordered_set.hpp>
#include <ctime>
#include <iostream>
#include "visualizer.h"


#include <type_traits>
#include <typeinfo>
#include <memory>
#include <string>
#include <cstdlib>

const int cwConstant = 5; //(mass * gravitational constant * cell length)/Power

enum OverrideFlags
{
    OF_RIVER_MARSH = 0x10,
    OF_INLAND = 0x20,
    OF_WATER_BASIN = 0x40
};

// Some constants
enum {
    IMAGE_DIM = 2048, // Width and height of the elevation and overrides image
    
    ROVER_X = 159,
    ROVER_Y = 1520,
    BACHELOR_X = 1303,
    BACHELOR_Y = 85,
    WEDDING_X = 1577,
    WEDDING_Y = 1294
};


boost::mt19937 random_generator;

// Distance traveled in the maze
typedef double distance;



#define GRID_RANK 2
typedef boost::grid_graph<GRID_RANK> grid;
typedef boost::graph_traits<grid>::vertex_descriptor vertex_descriptor;
typedef boost::graph_traits<grid>::vertices_size_type vertices_size_type;

typedef boost::graph_traits<grid>::edge_descriptor edge_descriptor;


typedef boost::graph_traits<grid>::edges_size_type edge_size_type;


struct vertex_hash:std::unary_function<vertex_descriptor, std::size_t> {
  std::size_t operator()(vertex_descriptor const& u) const {
    std::size_t seed = 0;
    boost::hash_combine(seed, u[0]);
    boost::hash_combine(seed, u[1]);
    return seed;
  }
};

typedef boost::unordered_set<vertex_descriptor, vertex_hash> vertex_set;
typedef boost::vertex_subset_complement_filter<grid, vertex_set>::type
        filtered_grid;

//typedef boost::property_map<grid, boost::edge_weight_t>::type WeightMap;
// A searchable maze
//
// The maze is grid of locations which can either be empty or contain a
// barrier.  You can move to an adjacent location in the grid by going up,
// down, left and right.  Moving onto a barrier is not allowed.  The maze can
// be solved by finding a path from the lower-left-hand corner to the
// upper-right-hand corner.  If no open path exists between these two
// locations, the maze is unsolvable.
//
// The maze is implemented as a filtered grid graph where locations are
// vertices.  Non-traversable vertices are filtered out of the graph.

class maze {
public:
  friend std::ostream& operator<<(std::ostream&, const maze&);
  friend maze random_maze(std::size_t, std::size_t);

  maze():m_grid(create_grid(0, 0)),m_barrier_grid(create_barrier_grid()) {};

  maze(std::size_t x, std::size_t y, const std::vector<uint8_t>& elevation):m_grid(create_grid(x, y)),
       m_barrier_grid(create_barrier_grid()),m_elev(elevation){};

  // The length of the maze along the specified dimension.
  vertices_size_type length(std::size_t d) const {return m_grid.length(d);}

  bool has_barrier(vertex_descriptor u) const {
    return m_barriers.find(u) != m_barriers.end();
  }

  bool solve(vertex_descriptor source, vertex_descriptor goal);
  bool solved() const {return !m_solution.empty();}
  bool solution_contains(vertex_descriptor u) const {
    return m_solution.find(u) != m_solution.end();
  }

  // Create the underlying rank-2 grid with the specified dimensions.
  grid create_grid(std::size_t x, std::size_t y) {
    boost::array<std::size_t, GRID_RANK> lengths = { {x, y} };
    return grid(lengths);
  }

  // Filter the barrier vertices out of the underlying grid.
  filtered_grid create_barrier_grid() {
    return boost::make_vertex_subset_complement_filter(m_grid, m_barriers);
  }

  double timeWeight(const vertex_descriptor& source, const vertex_descriptor& target, const std::vector<uint8_t>& elevation)
  {
    double stepTime = 0;
    bool diag = (abs(source[0]-target[0]) == abs(source[1]-target[1])) ? 1 : 0;
    auto sourceElevation = static_cast<int>(elevation[source[0]+source[1]*IMAGE_DIM]);
    auto targetElevation = static_cast<int>(elevation[target[0]+target[1]*IMAGE_DIM]);
  
    //check for water or flat
    if(sourceElevation == 0 || targetElevation == 0)
    {
      std::cout<< "ERROR! elevation of path is 0";
      return 0;
    }
    
    //std::cout << "source elevation: " << sourceElevation << std::endl;
    int delta = targetElevation - sourceElevation;
  
    if(!diag)
    {
      if(delta == 0)
      {
        stepTime += 1;
      }
      else if(delta > 0 )
        {
          stepTime +=  sqrt(1 + 0.003937*pow(delta,2)) + cwConstant*0.0627455*delta;
        }
      else
      { 
          stepTime += sqrt(1 + 0.003937*pow(delta,2)) - cwConstant*0.0627455*delta;
      }
      
    }
    else{
      if(delta == 0)
      {
        stepTime += 1.414;
      }
      else if(delta > 0 )
        {
          stepTime +=  sqrt(2 + 0.003937*pow(delta,2)) + cwConstant*0.0627455*delta;
        }
      else
      { 
          stepTime += sqrt(2 + 0.003937*pow(delta,2)) - cwConstant*0.0627455*delta;
      }
    }
    return stepTime;
  }


  // The grid underlying the maze
  grid m_grid;

  std::vector<uint8_t> m_elev;

  // The underlying maze grid with barrier vertices filtered out
  filtered_grid m_barrier_grid;
  // The barriers in the maze
  vertex_set m_barriers;
  // The vertices on a solution path through the maze
  vertex_set m_solution;
  // The length of the solution path
  distance m_solution_length;

};




class manhattan_heuristic:
public boost::astar_heuristic<filtered_grid, double>
{
public:
manhattan_heuristic(vertex_descriptor goal):m_goal(goal) {};

double operator()(vertex_descriptor v) {
return 1*(double(abs(m_goal[0] - v[0]) + double(abs(m_goal[1] - v[1]))));
}

private:
vertex_descriptor m_goal;
};


// Exception thrown when the goal vertex is found
struct found_goal {};

// Visitor that terminates when we find the goal vertex
struct astar_goal_visitor:public boost::default_astar_visitor {
  astar_goal_visitor(vertex_descriptor goal):m_goal(goal) {};

  void examine_vertex(vertex_descriptor u, const filtered_grid&) {
    if (u == m_goal)
      throw found_goal();
  }

private:
  vertex_descriptor m_goal;
};



// Solve the maze using A-star search.  Return true if a solution was found.
bool maze::solve(vertex_descriptor source, vertex_descriptor goal) {
  //boost::static_property_map<distance> weight(1);
  auto weight = boost::make_function_property_map<filtered_grid::edge_descriptor>([this](filtered_grid::edge_descriptor e) {
        return timeWeight(boost::source(e, m_barrier_grid), boost::target(e, m_barrier_grid), m_elev);});
  // The predecessor map is a vertex-to-vertex mapping.
  typedef boost::unordered_map<vertex_descriptor,
                               vertex_descriptor,
                               vertex_hash> pred_map;
  pred_map predecessor;
  boost::associative_property_map<pred_map> pred_pmap(predecessor);
  // The distance map is a vertex-to-distance mapping.
  typedef boost::unordered_map<vertex_descriptor,
                               distance,
                               vertex_hash> dist_map;
  dist_map distance;
  boost::associative_property_map<dist_map> dist_pmap(distance);

  manhattan_heuristic heuristic(goal);
  astar_goal_visitor visitor(goal);

  try {
    astar_search(m_barrier_grid, source, heuristic,
                 boost::weight_map(weight).
                 predecessor_map(pred_pmap).
                 distance_map(dist_pmap).
                 visitor(visitor) );  
    
  } catch(found_goal fg) {
    // Walk backwards from the goal through the predecessor chain adding
    // vertices to the solution path.
    for (vertex_descriptor u = goal; u != source; u = predecessor[u])
      m_solution.insert(u);
    m_solution.insert(source);
    m_solution_length = distance[goal];
    return true;
  }

  return false;
}


// Generate a maze with a random assignment of barriers.
maze make_maze(std::size_t x, std::size_t y, const std::vector<uint8_t>& overrides, std::vector<uint8_t>& elevation) {
  maze m(IMAGE_DIM, IMAGE_DIM, elevation);
  auto Obegin = overrides.begin();
  vertex_descriptor u;
  int count = 0;
  auto Ebegin = elevation.begin();
  for(auto i = overrides.begin(); i != overrides.end(); ++i)
  {
      if((*i & (OF_WATER_BASIN | OF_RIVER_MARSH)) || (elevation[i-Obegin] == 0))
      {
          count++;
          auto xBarr = (i - Obegin)%IMAGE_DIM;
          auto yBarr = ((i - Obegin) - xBarr)/IMAGE_DIM;

          //std::cout << "barriers are at " << xBarr <<", "<<yBarr <<std::endl;

          u = vertex((i-Obegin), m.m_grid);
          m.m_barriers.insert(u);
      }
    }
    std::cout<<std::endl;
    std::cout << "Number of non-traversable cells: " << count << std::endl;
  return m;
}




#endif