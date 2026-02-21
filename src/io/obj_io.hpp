#pragma once

#include "io/io_interfaces.hpp"

namespace transformer
{

class ObjMeshReader : public IMeshReader
{
public:
    Mesh read(const std::string& mesh_file, Profiler& profiler) const override;
};

class ObjMeshWriter : public IMeshWriter
{
public:
    void write(const std::string& output_file, const Mesh& mesh, Profiler& profiler) const override;
};

} // namespace transformer
