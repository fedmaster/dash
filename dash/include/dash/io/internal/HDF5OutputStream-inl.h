#ifndef DASH__IO__INTERNAL__HDF5_OUTPUT_STREAM_H__
#define DASH__IO__INTERNAL__HDF5_OUTPUT_STREAM_H__


#include <dash/io/HDF5OutputStream.h>
#include <dash/io/StoreHDF.h>

#include <dash/Matrix.h>



namespace dash {
namespace io {


HDF5OutputStream & operator<< (
  HDF5OutputStream & os,
  const HDF5Table & tbl)
{
  os._table = tbl._table;
  return os;
}

HDF5OutputStream & operator<< (
  HDF5OutputStream & os,
  HDF5Options opts)
{
  os._foptions = opts._foptions;
  return os;
}

template <
  typename value_t,
  dim_t    ndim,
  typename index_t,
  class    pattern_t >
inline HDF5OutputStream & operator<< (
  HDF5OutputStream & os,
  dash::Matrix < value_t,
  ndim,
  index_t,
  pattern_t > matrix)
{
  dash::util::StoreHDF::write(
    matrix,
    os._filename,
    os._table,
    os._foptions);
  return os;
}

} // namespace io
} // namespace dash

#endif // DASH__IO__INTERNAL__HDF5_OUTPUT_STREAM_H__
