// -*- C++ -*-

#ifndef V3D_GEOM_UTILITIES_H
#define V3D_GEOM_UTILITIES_H

#include <estimator/v3d_code/v3d_linear.h>
#include <estimator/v3d_code/v3d_linear_tnt.h>
#include <estimator/v3d_code/v3d_cameramatrix.h>
#include <set>
using std::set;

namespace V3D
{

   struct PointMeasurement
   {
         V3D::Vector2f pos;
         int           view, id;

         PointMeasurement()
            : id(-1)
         { }

         PointMeasurement(V3D::Vector2f const& pos_, int view_, int id_ = -1)
            : pos(pos_), view(view_), id(id_)
         { }

         bool operator==(PointMeasurement const& rhs) const
         {
            // Note: if view number and feature id are the same, we assume that the
            // 2D position is the same.
            return (this->id == rhs.id) && (this->view == rhs.view);
         }

   }; // end struct PointMeasurement

   struct PointCorrespondence
   {
         PointMeasurement left, right;

         PointCorrespondence() { }

         PointCorrespondence(PointMeasurement const& l, PointMeasurement const& r)
            : left(l), right(r)
         { }

         bool operator==(PointCorrespondence const& rhs) const
         {
            return (this->left == rhs.left) && (this->right == rhs.right);
         }

   }; // end struct PointCorrespondence

   struct TriangulatedPoint
   {
         V3D::Vector3d                 pos;
         std::vector<PointMeasurement> measurements;

         TriangulatedPoint() : measurements()
         {
            V3D::makeZeroVector(pos);
         }

         TriangulatedPoint(V3D::Vector3d const& p, std::vector<PointMeasurement> const& ms)
            : pos(p), measurements(ms)
         { }

         static void connectTracks(std::vector<PointCorrespondence> const& corrs,
                                   std::vector<TriangulatedPoint>& points,
                                   size_t nRequiredMeasurements = 2);

   }; // end struct TriangulatedPoint

   // Logical filtering of 3D points: check if every view appears at most once in the measurements
   inline void
   filterConsistentSparsePoints(std::vector<V3D::TriangulatedPoint>& model)
   {
      using namespace std;

      vector<TriangulatedPoint> const origModel(model);

      model.clear();

      for (size_t j = 0; j < origModel.size(); ++j)
      {
         TriangulatedPoint const& X = origModel[j];
         set<int> views;
         for (size_t k = 0; k < X.measurements.size(); ++k)
         {
            int const view = X.measurements[k].view;
            if (views.find(view) != views.end())
               goto loop_end;

            views.insert(view);
         } // end for (k)
         model.push_back(X);
      loop_end:;
      } // end for(j)
   } // end filterConsistentSparsePoints()

//======================================================================

   struct BundlePointStructure
   {
         BundlePointStructure()
         { }

         BundlePointStructure(std::vector<TriangulatedPoint> const& reconstruction,
                              size_t nRequiredMeasurements = 2)
         {
            int const nPoints = static_cast<int>(reconstruction.size());

            for (int i = 0; i < nPoints; ++i)
            {
               TriangulatedPoint const& X = reconstruction[i];
               if (X.measurements.size() >= nRequiredMeasurements)
               {
                  for (size_t k = 0; k < X.measurements.size(); ++k)
                  {
                     Vector2f const& p = X.measurements[k].pos;
                     measurements.push_back(makeVector2<double>(p[0], p[1]));
                     measurementIds.push_back(X.measurements[k].id);
                     correspondingView.push_back(X.measurements[k].view);
                     correspondingPoint.push_back(static_cast<int>(points3d.size()));
                  }
                  points3d.push_back(X.pos);
                  xsIndices.push_back(i);
               }
            } // end for (p)
         } // end VRBundlePointStructure()

         void createPointStructure(std::vector<TriangulatedPoint>& reconstruction,
                                   bool createFromScratch = true)
         {
            size_t const nPoints = points3d.size();

            if (createFromScratch)
            {
               reconstruction.resize(nPoints);
               for (size_t i = 0; i < nPoints; ++i)
               {
                  TriangulatedPoint& X = reconstruction[i];
                  X.measurements.clear();
                  X.pos = points3d[i];
               }

               for (size_t k = 0; k < measurements.size(); ++k)
               {
                  int i = correspondingPoint[k];
                  PointMeasurement m;
                  m.id = measurementIds[k];
                  m.view = correspondingView[k];
                  copyVector(measurements[k], m.pos);
                  reconstruction[i].measurements.push_back(m);
               }
            }
            else
            {
               size_t m = 0;
               for (size_t i = 0; i < nPoints; ++i)
               {
                  TriangulatedPoint& X = reconstruction[xsIndices[i]];
                  X.pos = points3d[i];

                  for (size_t k = 0; k < X.measurements.size(); ++k, ++m)
                     copyVector(measurements[m], X.measurements[k].pos);
               }
            } // end if (createFromScratch)
         } // end createPointStructure()

         std::vector<Vector3d> points3d;
         std::vector<Vector2d> measurements;
         std::vector<int>      measurementIds;
         std::vector<int>      correspondingView;
         std::vector<int>      correspondingPoint;
         std::vector<int>      xsIndices;
   }; // end struct BundleReconstruction

   template <typename Mat>
   inline Vector3d
   triangulateLinear(Mat const& P0, Mat const& P1, PointCorrespondence const& corr)
   {
      assert(P0.num_rows() == 3);
      assert(P0.num_cols() == 4);
      assert(P1.num_rows() == 3);
      assert(P1.num_cols() == 4);

      Matrix<double> A(4, 3);
      Vector<double> b(4);

      {
         Matrix3x4d const& P = P0;
         double x = corr.left.pos[0];
         double y = corr.left.pos[1];

         A[0][0] = x*P[2][0] - P[0][0];
         A[0][1] = x*P[2][1] - P[0][1];
         A[0][2] = x*P[2][2] - P[0][2];

         A[1][0] = y*P[2][0] - P[1][0];
         A[1][1] = y*P[2][1] - P[1][1];
         A[1][2] = y*P[2][2] - P[1][2];

         b[0] = P[0][3] - x*P[2][3];
         b[1] = P[1][3] - y*P[2][3];
      }

      {
         Matrix3x4d const& P = P1;
         double x = corr.right.pos[0];
         double y = corr.right.pos[1];

         A[2][0] = x*P[2][0] - P[0][0];
         A[2][1] = x*P[2][1] - P[0][1];
         A[2][2] = x*P[2][2] - P[0][2];

         A[3][0] = y*P[2][0] - P[1][0];
         A[3][1] = y*P[2][1] - P[1][1];
         A[3][2] = y*P[2][2] - P[1][2];

         b[2] = P[0][3] - x*P[2][3];
         b[3] = P[1][3] - y*P[2][3];
      }

      QR<double> qr(A);
      Vector<double> const X = qr.solve(b);

      return makeVector3(X[0], X[1], X[2]);
   } // end triangulateLinear()

   inline Vector3d
   triangulateLinear(std::vector<Matrix3x4d> const& Ps, std::vector<PointMeasurement> const& ms)
   {
      int const K     = static_cast<int>(ms.size());
      int const nRows = 2 * K;
      int const nCols = 3;

      Matrix<double> A(nRows, nCols);
      Vector<double> b(nRows);

      for (int i = 0; i < K; ++i)
      {
         float x = ms[i].pos[0];
         float y = ms[i].pos[1];

         Matrix3x4d const& P = Ps[ms[i].view];

         A[2*i+0][0] = x*P[2][0] - P[0][0];
         A[2*i+0][1] = x*P[2][1] - P[0][1];
         A[2*i+0][2] = x*P[2][2] - P[0][2];

         A[2*i+1][0] = y*P[2][0] - P[1][0];
         A[2*i+1][1] = y*P[2][1] - P[1][1];
         A[2*i+1][2] = y*P[2][2] - P[1][2];

         b[2*i+0] = P[0][3] - x*P[2][3];
         b[2*i+1] = P[1][3] - y*P[2][3];
      } // end for (i)

      QR<double> qr(A);
      Vector<double> const X = qr.solve(b);

      return makeVector3(X[0], X[1], X[2]);
   } // end triangulateLinear()

   inline Vector3d
   triangulateLinear(std::vector<CameraMatrix> const& cams, std::vector<PointMeasurement> const& ms)
   {
      int const N = static_cast<int>(cams.size());
      std::vector<Matrix3x4d> Ps(N);
      for (int i = 0; i < N; ++i)
         Ps[i] = cams[i].getProjection();
      return triangulateLinear(Ps, ms);
   } // end triangulateLinear()

   inline Vector3d
   triangulateLinear(std::vector<Matrix3x4d> const& Ps, std::vector<bool> const& isValidMatrix,
                     std::vector<PointMeasurement> const& ms)
   {
      assert(isValidMatrix.size() == Ps.size());

      int const K     = static_cast<int>(ms.size());
      int const nRows = 2 * K;
      int const nCols = 3;

      Matrix<double> A(nRows, nCols);
      Vector<double> b(nRows);

      for (int i = 0; i < K; ++i)
      {
         if (!isValidMatrix[i]) continue;

         float x = ms[i].pos[0];
         float y = ms[i].pos[1];

         Matrix3x4d const& P = Ps[ms[i].view];

         A[2*i+0][0] = x*P[2][0] - P[0][0];
         A[2*i+0][1] = x*P[2][1] - P[0][1];
         A[2*i+0][2] = x*P[2][2] - P[0][2];

         A[2*i+1][0] = y*P[2][0] - P[1][0];
         A[2*i+1][1] = y*P[2][1] - P[1][1];
         A[2*i+1][2] = y*P[2][2] - P[1][2];

         b[2*i+0] = P[0][3] - x*P[2][3];
         b[2*i+1] = P[1][3] - y*P[2][3];
      } // end for (i)

      QR<double> qr(A);
      Vector<double> const X = qr.solve(b);

      return makeVector3(X[0], X[1], X[2]);
   } // end triangulateLinear()

   enum
   {
      V3D_CONSISTENT_ROTATION_METHOD_SVD = 0,     // direct dense SVD approach
      V3D_CONSISTENT_ROTATION_METHOD_SVD_ATA = 1, // SVD on A^T*A, with sparse matrix multiplication
      V3D_CONSISTENT_ROTATION_METHOD_EIG_ATA = 2, // Eigendecomposition on A^T*A, with sparse matrix multiplication
      V3D_CONSISTENT_ROTATION_METHOD_SPARSE_EIG = 3 // Eigendecomposition using ARPACK
   };

   void computeConsistentRotations(int const nViews,
                                   std::vector<Matrix3x3d> const& relativeRotations,
                                   std::vector<std::pair<int, int> > const& viewPairs,
                                   std::vector<Matrix3x3d>& rotations,
                                   int method = V3D_CONSISTENT_ROTATION_METHOD_EIG_ATA);

# if defined(V3DLIB_ENABLE_SUITESPARSE)
   void refineConsistentRotations(int const nViews,
                                  std::vector<Matrix3x3d> const& relativeRotations,
                                  std::vector<std::pair<int, int> > const& viewPairs,
                                  std::vector<Matrix3x3d>& rotations);
#endif

   Matrix3x3d computeIntersectionCovariance(vector<Matrix3x4d> const& projections,
                                            vector<PointMeasurement> const& measurements,
                                            double sigma);

   double computeIntersectionRoundness(vector<Matrix3x4d> const& projections,
                                       vector<PointMeasurement> const& measurements,
                                       double sigma);

} // end namespace V3D

#endif
