/*
  http://doc.qt.io/qt-5/qmake-test-function-reference.html#qtcompiletest-test
*/
#include <mapnik/map.hpp>

#include <mapnik/version.hpp>
#include <mapnik/datasource_cache.hpp>
#include <mapnik/agg_renderer.hpp>
#include <mapnik/load_map.hpp>
#include <mapnik/projection.hpp>
#include <mapnik/value.hpp>
#include <mapnik/image_util.hpp>


#if MAPNIK_VERSION < 300000
#error "Unsupported mapnik version" MAPNIK_VERSION
#endif


int main()
{
	mapnik::Map * map = new mapnik::Map;
	delete map;
	return 0;
}
