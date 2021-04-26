#include <iostream>
#include <fstream>
#include <memory>
#include <chrono>
#include <osmium/handler.hpp>
#include <osmium/io/any_input.hpp>
#include <osmium/osm/node.hpp>
#include <osmium/osm/way.hpp>
#include <osmium/visitor.hpp>
#include <string.h>
#include <pqxx/pqxx>
#include <filesystem>

std::string node2Sql(const osmium::Node& node) {   
  long id = node.id();
  std::string lon = std::to_string(node.location().lon());
  std::string lat = std::to_string(node.location().lat());

  std::string geom = "ST_GEOMFROMTEXT(\'POINT (" + lon + " " + lat + ")\', 4326)";
  std::string query = "INSERT INTO osm.points(id, geom) VALUES (" + std::to_string(id) + ", " + geom + ")";
  return query;
}

std::string way2PolygonSql(long id, std::string& locs) {
  std::string geom = "ST_GEOMFROMTEXT(\'POLYGON((" + locs + "))\', 4326)";
  std::string query = "INSERT INTO osm.polygons(id, geom) VALUES (" + std::to_string(id) + ", " + geom + ")";

  return query;
}

std::string way2LineSql(long id, std::string& locs) {
  std::string geom = "ST_GEOMFROMTEXT(\'LINESTRING(" + locs + ")\', 4326)";
  std::string query = "INSERT INTO osm.lines(id, geom) VALUES (" + std::to_string(id) + ", " + geom + ")";

  return query;
}

long runQuery(std::string query, std::shared_ptr<pqxx::connection> sdb) {
  auto start = std::chrono::high_resolution_clock::now();

  pqxx::work worker(*sdb);
  worker.exec(query);
  worker.commit();

  auto stop = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::microseconds>(stop - start);
  long count = duration.count();

  //std::cout << "Time taken by function: " << count << " microseconds" << std::endl;

  return count;
}


class MyHandler : public osmium::handler::Handler {
public:
  MyHandler(std::string db_url) {
    std::cout << "URL: " << db_url << std::endl;
    sdb = std::make_shared<pqxx::connection>(db_url);
  }

  void way(const osmium::Way& way) {
    const osmium::WayNodeList& ref_nodes = way.nodes();

    long first, last;
    std::string locs;

    for (auto it = ref_nodes.begin(); it != ref_nodes.end(); ++it) {
      if (it == ref_nodes.begin()) {
        first = it->ref();
      }

      auto loc = refs[it->ref()];
      locs += std::to_string(loc.first) + " " + std::to_string(loc.second);

      if (it == ref_nodes.end() - 1) {
        last = it->ref();
      } else {
        locs += ",";
      }
    }

    std::string query;
    long id = way.id();
    if (first == last) {
      query = way2PolygonSql(id, locs);
    } else {
      query = way2LineSql(id, locs);
    }

    total_ways += runQuery(query, sdb);
    count_ways += 1;
  }

  void node(const osmium::Node& node) {
    std::string query = node2Sql(node);

    if (node.tags().size() == 0) {
      auto loc = node.location();
      long id = node.id();

      refs.emplace(id, std::make_pair(loc.lon(), loc.lat()));
    } else {
      total_nodes += runQuery(query, sdb);
      count_nodes += 1;
    }
  }

  std::map<long, std::pair<double, double>> refs;
  std::shared_ptr<pqxx::connection> sdb;
  long count_ways = 0;
  long count_nodes = 0;

  long total_nodes = 0;
  long total_ways = 0;

};

int main(int argc, char* argv[]) {
  auto otypes = osmium::osm_entity_bits::node | osmium::osm_entity_bits::way;

  std::string path = argv[1];
  std::string db_url = std::getenv("DB_URL");

  osmium::io::Reader reader{path, otypes};

  MyHandler handler(db_url);
  osmium::apply(reader, handler);
  reader.close();

  std::cout << "File = " <<  path << std::endl;

  std::cout << "nodes" << std::endl << "======" << std::endl
    << "total time(us) = " << handler.total_nodes << std::endl
    << "Number of nodes = " << handler.count_nodes << std::endl << std::endl;

  std::cout << "ways" << std::endl << "======" << std::endl
    << "total time(us) = " << handler.total_ways << std::endl
    << "Number of ways = " << handler.count_ways << std::endl << std::endl;


  long total_time = handler.total_nodes + handler.total_ways;
  long count_total = handler.count_nodes + handler.count_ways;

  double avg_time = total_time / count_total;
  std::cout << "Average time(us) = " << avg_time << std::endl;

  // Store results.
  std::string output_file = "results.csv";
  bool exists = std::filesystem::exists(output_file);

  std::ofstream file;
  file.open(output_file, std::ios::out | std::ios::app);
  if (!exists) {
    file << "file_name,db_url,total_nodes,count_nodes,total_ways,count_ways,total_time,count_total,average_time\n";
  }

  std::string row = path + "," + db_url + "," + std::to_string(handler.total_nodes) + "," + std::to_string(handler.count_nodes) + "," +
    std::to_string(handler.total_ways) + "," + std::to_string(handler.count_ways) + "," + std::to_string(total_time)
    + "," + std::to_string(count_total) + "," + std::to_string(avg_time) + "\n";
  file << row;

  file.close();

}