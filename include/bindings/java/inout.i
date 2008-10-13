%apply unsigned long long { uint64 };
%apply long long { int64 };
%apply long { uint32, uint, int32, int, size_t };
%apply int { uint16, int16 };
%apply short { uint8 };
%apply signed char { int8 };

%apply signed char * OUTPUT { int8 & v };
%apply short * OUTPUT { uint8 & v };
%apply int * OUTPUT { int16 & v, uint16 & v };
%apply long * OUTPUT { int32 & v, int & v, uint32 & v, uint & v };
%apply long long * OUTPUT { int64 & v };
%apply unsigned long long * OUTPUT { uint64 & v };
%apply bool * OUTPUT { bool & v, bool & coplanar };
%apply float * OUTPUT { float & v, float & valid_radius };
%apply double * OUTPUT { double & v };
%apply csRef<iEvent> & OUTPUT { csRef<iEvent> & v };
%apply csRef<iBase> & OUTPUT { csRef<iBase> & v };
%apply char * OUTPUT { char *& v };
%apply (void *&OUTPUT, size_t &OUTPUT) { (const void *& v, size_t & size) };
%apply (void *INPUT,size_t INPUT) { (const void *v, size_t size) };
%apply (csTriangleMinMax *INPUT,size_t INPUT) { (csTriangleMinMax* tris, size_t tri_count) };
%apply (csTriangleMinMax *&OUTPUT,size_t & OUTPUT) { (csTriangleMinMax*& tris, size_t & tri_count) };
%apply (csTriangleMeshEdge *INPUT,size_t INPUT) { (csTriangleMeshEdge* edges, size_t num_edges) };
%apply (csPlane3 *INPUT,size_t INPUT) { (csPlane3* planes, size_t num_vertices) };
%apply (iTriangleMesh *INPUT,size_t &INPUT) { (iTriangleMesh* edges, size_t & num_edges) };
%apply (csTriangleMeshEdge *OUTPUT,size_t &OUTPUT) { (csTriangleMeshEdge* results, size_t & num_results) };
%apply (csVector3 *OUTPUT) {csVector3* normals,csVector3* vertices};
%apply (csPlane3 *OUTPUT) {csPlane3* planes};
%apply (csArray<csArray<unsigned int> >& INPUT) {csArray<csArray<unsigned int> >& boneIndices};
%apply (csArray<iRenderBuffer*>& INPUT) {csArray<iRenderBuffer*>& indices};
%apply (csArray<csTriangle>& INOUT) {csArray<csTriangle>& newtris};
%apply (csArray<csArray<int> > * OUTPUT) {csArray<csArray<int> > *};
%apply (bool *OUTPUT,size_t &OUTPUT) { (bool* outline_verts,size_t & num_outline_verts) };
%apply (size_t *OUTPUT,size_t &OUTPUT) { (size_t* outline_edges, size_t& num_outline_edges) };
%apply (csPlane3 *&OUTPUT,size_t &OUTPUT) { (csPlane3** planes,size_t & plane_count) };
%apply (csTriangleMinMax *&OUTPUT,size_t &OUTPUT) { (csTriangleMinMax** tris, size_t& tri_count) };