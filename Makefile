default:
	g++ test.cpp -o test --std=c++17 -lpthread -lz -lexpat -lbz2 -lpqxx -lpq

download:
	wget https://download.geofabrik.de/south-america/colombia-latest.osm.pbf

clean:
	rm results.csv

run_local:
	DB_URL="postgresql://up:up@localhost:5432/run_tests" ./test ~/Downloads/nauru-latest.osm.pbf
