#include "FileUtils.hpp"
#include <fstream>
#include <sstream>

using namespace std;
namespace FileUtils{

std::string readFile2String(const std::string &file){
    ifstream ifile(file);
    if(!ifile.is_open()){
        return {};
    }

    std::stringstream ss;
    ss << ifile.rdbuf();
    return ss.str();
}

}

