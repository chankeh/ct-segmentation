#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <numeric>
#include <iostream>

#include <CImg.h>

#define cimg_display 0
#define cimg_verbosity 0

using namespace std;
using namespace cimg_library;

// Description of a blob
struct Blob {
  Blob() : area(0) {}  
  union {
    unsigned int area; ///< Area (moment 00).
    unsigned int m00; ///< Moment 00 (area).
  };
  unsigned int minx; ///< X min.
  unsigned int maxx; ///< X max.
  unsigned int miny; ///< Y min.
  unsigned int maxy; ///< Y max.
  pair<double, double> centroid; ///< Centroid.
  double m10; ///< Moment 10.
  double m01; ///< Moment 01.
  vector<char> contour; ///< Contour as a chain code.
  // x and y coordinates of pixels of the blob
  vector<unsigned> x, y; 
  // x and y coordinates of the blob's border
  vector<unsigned> border_x, border_y;
  // Neighbours counter. 
  // -1 for pixels out of the image
  map<int, unsigned> neighbor;
};

//! For searching neighbohood color with maximum area in map container
typedef std::map<int, unsigned> M;
bool value_comparer(M::value_type &el1, M::value_type &el2) {
  return el1.second < el2.second;
}

//! Detection of regions (blobs) on an image and calculating blob's characteristics
/**
  \param  labeled_slice Labeled image, result of CImg<> label method
  \result Map of blobs
**/
map <unsigned, Blob> FindBlobs(const CImg<> &labeled_slice) {
  map <unsigned, Blob> blobs;
  // Area and coordinates of each blob
  cimg_forXY(labeled_slice, x, y) {
    unsigned val = static_cast<unsigned>(labeled_slice(x,y)); 
    if (0 != val) { // 0 - ��� ���
      blobs[val].area++;
      blobs[val].x.push_back(x);
      blobs[val].y.push_back(y);
    }
  }
  map<unsigned, Blob>::iterator it;
  // Image moments
  for (it = blobs.begin(); it != blobs.end(); ++it) {
    vector<unsigned> coords = it->second.x;
    it->second.minx = *(std::min_element(coords.begin(), coords.end()));
    it->second.maxx = *(std::max_element(coords.begin(), coords.end()));
    it->second.m10 = std::accumulate(coords.begin(), coords.end(), 0);
    coords = it->second.y;
    it->second.miny = *(std::min_element(coords.begin(), coords.end()));
    it->second.maxy = *(std::max_element(coords.begin(), coords.end()));
    it->second.m01 = std::accumulate(coords.begin(), coords.end(), 0);
    it->second.centroid.first = it->second.m10 / it->second.m00;
    it->second.centroid.second = it->second.m01 / it->second.m00;
  }
  // Border tracing, 4-connectivity
  /*
            1
            |
        2 --x-- 0
            |
            3    
  */
  int direction_ways[4][2] = {{1, 0}, {0, -1}, {-1, 0}, {0, 1}};
  int im_w = labeled_slice.width(), im_h = labeled_slice.height();

  for (it = blobs.begin(); it != blobs.end(); ++it) {
    unsigned current_x = it->second.x[0],
             current_y = it->second.y[0];

    it->second.border_x.push_back(current_x);
    it->second.border_y.push_back(current_y);

    // 4 neighbours of single-pixel region
    if (1 == it->second.area) {
      for (unsigned i = 0; i < 4; ++i) {
        int  new_x = current_x + direction_ways[i][0],
             new_y = current_y + direction_ways[i][1];          
        int curr_pos_value = static_cast<int>(((0 > new_x) || (0 > new_y) || (im_w <= new_x) || (im_h <= new_y))? -1 : labeled_slice(new_x, new_y));
        it->second.neighbor[curr_pos_value]++;
        it->second.border_x.push_back(new_x);
        it->second.border_y.push_back(new_y);
      }
    } else {
      unsigned direction = 0; //initial value if the border is detected in 4-connectivity
      unsigned length = 1;
      while (true) {
        direction = (direction + 3) % 4;
        unsigned dir = direction;
        int  new_x = current_x + direction_ways[dir][0],
             new_y = current_y + direction_ways[dir][1];
        // Border neighbour's effect. If current pixel not in the image will not consider it
        int curr_pos_value = static_cast<int>(((0 > new_x) || (0 > new_y) || (im_w <= new_x) || (im_h <= new_y))? -1 : labeled_slice(new_x, new_y));
        while (curr_pos_value != it->first) {
          it->second.neighbor[curr_pos_value]++;
          dir = (dir+1) % 4;
          new_x = current_x + direction_ways[dir][0];
          new_y = current_y + direction_ways[dir][1];
          if ((0 > new_x) || (0 > new_y) || (im_w <= new_x) || (im_h <= new_y)) continue;
          curr_pos_value = static_cast<int>(labeled_slice(new_x, new_y));
        } // while (curr_pos_value != it->first)
        if ( (length > 1) &&  
              (new_x == it->second.border_x[1]) && (new_y == it->second.border_y[1]) &&
              current_x == it->second.border_x[0] && (current_y == it->second.border_y[0]))  {
          break;
        }
        current_x = new_x;
        current_y = new_y;
        it->second.border_x.push_back(current_x);
        it->second.border_y.push_back(current_y);
        ++length;
        direction = dir;
        it->second.contour.push_back(direction);
      } // while (true) 
    }
  }
  return blobs;
}

//! Blob preprocessing
/**
  Deleting of nested blobs
  \param  blobs List of blobs returned by FindBlobs function
  \param  image_width Image width
  \result List of blobs without nested ones
**/
map <unsigned, Blob> ProcessBlobs(const map<unsigned, Blob> &blobs, const unsigned &image_width) {
  map<unsigned, Blob> processed_blobs(blobs);
  map<unsigned, Blob>::iterator it = processed_blobs.begin();
  map<unsigned, unsigned> deleted_blobs; 
  map<unsigned, unsigned>::iterator pos; // for searching among deleted items
                                
  while(it != processed_blobs.end()) {
    Blob blobik = it->second;
    map<int, unsigned> neighbours = blobik.neighbor;
    unsigned neighbour_color =  std::max_element(neighbours.begin(), neighbours.end(), value_comparer)->first;
    if (0 != neighbour_color) {
      pos = deleted_blobs.find(neighbour_color);
      if (pos != deleted_blobs.end()) {
        neighbour_color = pos->second;
      } else {
        deleted_blobs[it->first] = neighbour_color;
      }
      it = processed_blobs.erase(it);
      for (unsigned j = 0; j < blobik.x.size(); ++j) {
        processed_blobs[neighbour_color].x.push_back(blobik.x[j]);
        processed_blobs[neighbour_color].y.push_back(blobik.y[j]);
        processed_blobs[neighbour_color].area++;
      }
    } else {
      it++;
    }
  }
  // Delete blobs with center of mass in extreme regions (image center or image border)
  const unsigned unit = image_width / 12; 
  // It seems convienet to define measure unit as 1/12 of whole image size

  it = processed_blobs.begin(); 
  while(it != processed_blobs.end()) {
    Blob blobik = it->second;
    pair<double, double> c = blobik.centroid;
    if ( (c.second >= 3*unit) && (c.second <= 7*unit) && // ������ � �����
         (c.first >= 2*unit) && (c.first <= 10*unit) ) { // ����� � ������
      it++; 
    } else {
      it = processed_blobs.erase(it);  
    }
  }  
  
  // ���������� ��������� ������� �����.
  // ���� ������ ������ 3, �� ��������� ���� - ������� � ����������� ��������
  // ���� ������ ������ 3, �� ��������� ���� � ������� �� �������� ��������.
  vector<unsigned> blobs_areas;
  for (it = processed_blobs.begin(); it != processed_blobs.end(); ++it) {
    blobs_areas.push_back(it->second.area);
  }
  if (blobs_areas.empty()) { // �� ��� ������, ���� ���������� ������ ������ �� �������
    blobs_areas.push_back(0);
  }
  sort(blobs_areas.begin(), blobs_areas.end(), std::greater<unsigned>()); // �� �������� ��������
  unsigned area_threshold, thresh_num;
  // ������� ���� � �������� ��������� ������� ������� �� �������� ������
  thresh_num = 0;
  area_threshold = blobs_areas[thresh_num];
  /* ������ ������� - ������ �����, ������ - ������ �����.
      ���� ��� ������� ������ ���������� �� �������, ��,
      ������ �����, ����� ���� � ������, ������� ��������� � ������������ ����� ���� ������� ����.
      � ���� ������ ������ ������� - ��� �����, ������ - ������ ����, ������� ����� ����� �������. 
  */
  if (blobs_areas.size() > 1) {
    double ratio1 = 1.0 * blobs_areas[0] / (blobs_areas[0] + blobs_areas[1]),
          ratio2 = 1.0 * blobs_areas[1] / (blobs_areas[0] + blobs_areas[1]);
    if (ratio1 < 5 * ratio2) { 
      // ������, � ������-�� ���� ������ �� ������, �� ������ �� �������. ���� ���.
      // ������� � �������� ���������� ���� ������.
      area_threshold = blobs_areas[1];
      --thresh_num;
    }
  }
  it = processed_blobs.begin(); 
  while(it != processed_blobs.end()) {
    Blob blobik = it->second; 
    if (blobik.area < area_threshold) {
      it = processed_blobs.erase(it);  
    } else {
      it++;
    }
  }
  return processed_blobs;
}

//! Lung segmentation
/**
  Segmentation of a CT slice
  \param  slice CT slice which should pe segmented
  \result Segmented image. �������, �� ������������� ������ ��������� (=0)
**/
CImg<> SegmentSlice(const CImg<> &slice) {
  CImg<short> segmented_slice;

  if (slice.is_empty()) {
    return segmented_slice;
  }

  // ��� ����������� ������� ����� ���������� �������� � 8-������� ������������ ���������
  segmented_slice = slice.get_normalize(0, 255);
  /* ����� ����� �������� �����, �� ������� ����� ����� ���������� ��������� ����������.
      ��������: ����� ��� �������� ����� (�������� ����� � ������ �� ������ �������� ����� �����),
                ����� ������� ����� ���� �������� ����� - ������ ��� ��������.
  */        
  CImg<unsigned> histo = segmented_slice.get_histogram(128, 0, 255);
histo.display_graph("", 3);
  histo.max() = 0; // ��� � ����, ������ ����� �������� �����
  histo.max() = 0; // ��� � ������ �����������, ��� ���� �� �����. ������ � ��������
  // �������� ����� �� ���������, ���� ����� � ����� � ����� �� �������
  unsigned saddle_point = 55, prev_x = 0;
  // ���� ���������� � 50, ��� ��� ��������� �������� ������ ������ ����� 100 (=50 *2������ �������� �����������)
  cimg_for_insideX(histo, x, 55) {
    unsigned curr_x = histo(x), before_x = histo(x-1), next_x = histo(x+1);
    // �������� ����� ���������� ��������� � �����������.
    // �� ������ ���� ������ �������. � ��������� ������ - curr_x ������ ������� �������� ������
    float diff = abs(1.0*before_x - 1.0*next_x);
    if (0 == prev_x) prev_x = curr_x;
    if ( (100 < curr_x ) && (4000 > curr_x) && (before_x >= curr_x)  && (next_x >= curr_x) && (diff < 500) ) {
      if (curr_x < prev_x) {
        saddle_point = x;
        break;
      }
      prev_x = curr_x;
    }
  } 
  // �� �������� ������� �� ��������� ������� ������� ��� �������������� � ��������������� �����������
  segmented_slice.resize_halfXY();
  // ���������, �������� �� ������, ����������� �� ������� ����
  CImg<> segmented1 = segmented_slice.get_blur_median(3).get_threshold(2*saddle_point).get_dilate(3).get_erode(3);
  // ������� � ������� ���� ������ ������ ���� �����, flood_fill
  unsigned char color[] = {1};
  segmented1.draw_fill(0, 0, color);
  segmented1.draw_fill(segmented1.width()-1, 0, color);
  segmented1.draw_fill(0, segmented1.height()-1, color);
  segmented1.draw_fill(segmented1.width()-1, segmented1.height()-1, color);
  // ������ ������������� ������
  CImg<> segmented1_labeled =  segmented1.get_label(false);
  map <unsigned, Blob> blobs = FindBlobs(segmented1_labeled);
  // ������� ��������� �����, ����� � ������������ ������, ����� ��������� �������
  map <unsigned, Blob> processed_blobs = ProcessBlobs(blobs, segmented1_labeled.width());
  // ���������� ���������� �����
  segmented1.fill(1);
  map<unsigned, Blob>::iterator it;
  for (it = processed_blobs.begin(); it != processed_blobs.end(); ++it) {
    vector<unsigned> x = it->second.x,
                     y = it->second.y;
    for (unsigned i=0; i < x.size(); ++i) {
      segmented1(x[i], y[i]) = 0;
    }    
  }
  return segmented1.resize_doubleXY();
}

CImg<> GetImgMask(const CImg<> &img) {
  int layers = img.depth();
  CImg<> res(img.width(), img.height(), layers, 1, 0);
  cimg_forZ(img, z) {
    CImg<> slice = SegmentSlice(img.get_slice(z));
    cimg_forXY(res, x, y) {
      res(x, y, z) = slice(x, y);
    }
  }
  return res;
}
 
CImg<> GetSegmentedImg(const CImg<> &img, const CImg<> &mask) {
  short min = img.min(); // Background color has lowest value
  CImg<> res(img.width(), img.height(), img.depth(), 1, 0);
  cimg_forXYZ(img, x, y, z) {
    res(x, y, z) = (0 == mask(x, y, z))? img(x, y, z) : min;
  }
  return res;
}

string ExtractDirectory(const string& path) {
  return path.substr(0, path.find_last_of('\\') + 1);
}

string ExtractFilename(const string& path) {
  return path.substr(path.find_last_of('\\') + 1);
}

string ChangeExtension(const string& path, const string& ext) {
  string filename = ExtractFilename(path);
  return ExtractDirectory(path) + filename.substr(0, filename.find_last_of( '.' )) + ext;
}

int main(int argc, char **argv) {
  CImg<short> img;
  if (argc > 1) {
    img.load_analyze(argv[1]);
  } else {
    cerr << "Too few parameters. Please specify any CT image (.hdr file).";
    exit(-1);
  }

  CImg<short> mask_img = GetImgMask(img);
  string out_fname = ChangeExtension(argv[1], "_mask");
  mask_img.save_analyze(out_fname.c_str());
  
  CImg<short> segmented_img = GetSegmentedImg(img, mask_img);
  out_fname = ChangeExtension(argv[1], "_segm");  
  segmented_img.save_analyze(out_fname.c_str());

  return 0;
}