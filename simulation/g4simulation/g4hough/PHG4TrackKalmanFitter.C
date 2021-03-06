/*!
 *  \file		PHG4TrackKalmanFitter.C
 *  \brief		Refit SvtxTracks with PHGenFit.
 *  \details	Refit SvtxTracks with PHGenFit.
 *  \author		Haiwang Yu <yuhw@nmsu.edu>
 */

#include "PHG4TrackKalmanFitter.h"

#include <fun4all/Fun4AllReturnCodes.h>
#include <fun4all/PHTFileServer.h>
#include <g4hough/SvtxCluster.h>
#include <g4hough/SvtxClusterMap.h>
#include <g4main/PHG4TruthInfoContainer.h>
#include <g4main/PHG4Particle.h>
#include <g4main/PHG4Particlev2.h>
#include <g4main/PHG4VtxPointv1.h>
#include <GenFit/FieldManager.h>
#include <GenFit/GFRaveVertex.h>
#include <GenFit/GFRaveVertexFactory.h>
#include <GenFit/MeasuredStateOnPlane.h>
#include <GenFit/RKTrackRep.h>
#include <GenFit/StateOnPlane.h>
#include <GenFit/Track.h>
#include <phgenfit/Fitter.h>
#include <phgenfit/PlanarMeasurement.h>
#include <phool/getClass.h>
#include <phool/phool.h>
#include <phool/PHCompositeNode.h>
#include <phool/PHIODataNode.h>
#include <phool/PHNodeIterator.h>
#include <iostream>
#include <map>
#include <utility>
#include <vector>

#include "TClonesArray.h"
#include "TMatrixDSym.h"
#include "TTree.h"
#include "TVector3.h"
#include "phgenfit/Track.h"
#include "SvtxTrack.h"
#include "SvtxTrack_v1.h"
#include "SvtxVertex_v1.h"
#include "SvtxTrackMap.h"
#include "SvtxTrackMap_v1.h"
#include "SvtxVertexMap_v1.h"

#define LogDebug(exp) std::cout << __FILE__ <<":"<< __LINE__ <<": " << exp <<std::endl

using namespace std;


//Rave
#include <rave/Version.h>
#include <rave/Track.h>
#include <rave/VertexFactory.h>
#include <rave/ConstantMagneticField.h>

//GenFit
#include <GenFit/GFRaveConverters.h>

class PHRaveVertexFactory {

public:
	//! ctor
	PHRaveVertexFactory(const int verbosity) {
		rave::ConstantMagneticField mfield(0., 0., 0.); // RAVE use Tesla
		_factory = new rave::VertexFactory(mfield, rave::VacuumPropagator(),
				"default", verbosity);

		IdGFTrackStateMap_.clear();
	}

	//! dotr
	~PHRaveVertexFactory() {
		clearMap();

		delete _factory;
	}

	void findVertices(std::vector<genfit::GFRaveVertex*>* vertices,
			const std::vector<genfit::Track*>& tracks,
			const bool use_beamspot = false) {

		clearMap();

		try {
			genfit::RaveToGFVertices(vertices,
					_factory->create(
							genfit::GFTracksToTracks(tracks, NULL,
									IdGFTrackStateMap_, 0), use_beamspot),
					IdGFTrackStateMap_);
		} catch (genfit::Exception & e) {
			std::cerr << e.what();
		}
	}

	void findVertices(std::vector<genfit::GFRaveVertex*>* vertices,
			const std::vector<genfit::Track*>& tracks,
			std::vector < genfit::MeasuredStateOnPlane* > & GFStates,
			const bool use_beamspot = false) {

		clearMap();

		try {
			genfit::RaveToGFVertices(vertices,
					_factory->create(
							genfit::GFTracksToTracks(tracks, &GFStates,
									IdGFTrackStateMap_, 0), use_beamspot),
					IdGFTrackStateMap_);
		} catch (genfit::Exception & e) {
			std::cerr << e.what();
		}
	}

private:
	void clearMap() {

		for (unsigned int i = 0; i < IdGFTrackStateMap_.size(); ++i)
			delete IdGFTrackStateMap_[i].state_;

		IdGFTrackStateMap_.clear();
	}

	std::map<int, genfit::trackAndState> IdGFTrackStateMap_;

	rave::VertexFactory* _factory;

};


/*
 * Constructor
 */
PHG4TrackKalmanFitter::PHG4TrackKalmanFitter(const string &name) :
		SubsysReco(name), _flags(NONE), _mag_field_re_scaling_factor(1.4/1.5), _reverse_mag_field(true), _fitter( NULL), _vertex_finder( NULL), _vertexing_method("mvf"), _truth_container(
				NULL), _clustermap(NULL), _trackmap(NULL), _vertexmap(NULL), _trackmap_refit(
				NULL), _vertexmap_refit(NULL), _do_eval(false), _eval_outname(
				"PHG4TrackKalmanFitter_eval.root"), _eval_tree(
		NULL), _tca_particlemap(NULL), _tca_vtxmap(NULL), _tca_trackmap(NULL), _tca_vertexmap(
		NULL), _tca_trackmap_refit(NULL), _tca_vertexmap_refit(NULL), _do_evt_display(
				false) {
	_event = 0;
}

/*
 * Init
 */
int PHG4TrackKalmanFitter::Init(PHCompositeNode *topNode) {
	cout << PHWHERE << " Openning file " << _eval_outname << endl;

	CreateNodes(topNode);

	//_fitter = new PHGenFit::Fitter("sPHENIX_Geo.root","sPHENIX.2d.root", 1.4 / 1.5);
	_fitter = PHGenFit::Fitter::getInstance("sPHENIX_Geo.root",
			"sPHENIX.2d.root", (_reverse_mag_field) ? -1.*_mag_field_re_scaling_factor : _mag_field_re_scaling_factor, "KalmanFitterRefTrack", "RKTrackRep",
			_do_evt_display);

	if (!_fitter) {
		cerr << PHWHERE << endl;
		return Fun4AllReturnCodes::ABORTRUN;
	}

	//LogDebug(genfit::FieldManager::getInstance()->getFieldVal(TVector3(0, 0, 0)).Z());


	_vertex_finder = new genfit::GFRaveVertexFactory(verbosity);
	//_vertex_finder->setMethod("kalman-smoothing:1"); //! kalman-smoothing:1 is the defaul method
	_vertex_finder->setMethod(_vertexing_method.data());
	//_vertex_finder->setBeamspot();

	//_vertex_finder = new PHRaveVertexFactory(verbosity);


	if (!_vertex_finder) {
		cerr << PHWHERE << endl;
		return Fun4AllReturnCodes::ABORTRUN;
	}

	if (_do_eval) {
		PHTFileServer::get().open(_eval_outname, "RECREATE");
		init_eval_tree();
	}

	return Fun4AllReturnCodes::EVENT_OK;
}

/*
 * process_event():
 *  Call user instructions for every event.
 *  This function contains the analysis structure.
 *
 */
int PHG4TrackKalmanFitter::process_event(PHCompositeNode *topNode) {
	_event++;
	if (_event % 1000 == 0)
		cout << PHWHERE << "Events processed: " << _event << endl;

	GetNodes(topNode);

	//! stands for Refit_GenFit_Tracks
	vector<genfit::Track*> rf_gf_tracks;
	rf_gf_tracks.clear();

	std::vector<genfit::GFRaveVertex*> rave_vertices;
	rave_vertices.clear();


	for(SvtxTrackMap::ConstIter iter = _trackmap->begin(); iter != _trackmap->end();++iter)
	{
		//! stands for Refit_PHGenFit_Track
		PHGenFit::Track* rf_phgf_track = ReFitTrack(iter->second);
		SvtxTrack* rf_track = MakeSvtxTrack(iter->second,rf_phgf_track);
		_trackmap_refit->insert(rf_track);
		rf_gf_tracks.push_back(rf_phgf_track->getGenFitTrack());

//		if(verbosity >= 2){
//			rf_phgf_track->getGenFitTrack()->Print();
//		}
	}

	//! add tracks to event display
	if(_do_evt_display)
		_fitter->getEventDisplay()->addEvent(rf_gf_tracks);

	//! find vertex using tracks
	_vertex_finder->findVertices(&rave_vertices,rf_gf_tracks);


	FillSvtxVertexMap(rave_vertices,rf_gf_tracks);

	if (_do_eval) {
		fill_eval_tree(topNode);
	}

	return Fun4AllReturnCodes::EVENT_OK;
}

/*
 * End
 */
int PHG4TrackKalmanFitter::End(PHCompositeNode *topNode) {

	if (_do_eval) {
		PHTFileServer::get().cd(_eval_outname);
		_eval_tree->Write();
	}

	if(_do_evt_display)
		_fitter->displayEvent();

	return Fun4AllReturnCodes::EVENT_OK;
}

/*
 * dtor
 */
PHG4TrackKalmanFitter::~PHG4TrackKalmanFitter()
{
	delete _fitter;
	delete _vertex_finder;
}

/*
 * fill_eval_tree():
 */
void PHG4TrackKalmanFitter::fill_eval_tree(PHCompositeNode *topNode) {
	//! Make sure to reset all the TTree variables before trying to set them.
	reset_eval_variables();

	int i = 0;
	for (PHG4TruthInfoContainer::ConstIterator itr =
			_truth_container->GetPrimaryParticleRange().first;
			itr != _truth_container->GetPrimaryParticleRange().second; ++itr)
		new ((*_tca_particlemap)[i++]) (PHG4Particlev2)(
				*dynamic_cast<PHG4Particlev2*>(itr->second));

	i = 0;
	for (PHG4TruthInfoContainer::ConstVtxIterator itr =
			_truth_container->GetVtxRange().first;
			itr != _truth_container->GetVtxRange().second; ++itr)
		new ((*_tca_vtxmap)[i++]) (PHG4VtxPointv1)(
				*dynamic_cast<PHG4VtxPointv1*>(itr->second));

	i = 0;
	for (SvtxTrackMap::ConstIter itr =
			_trackmap->begin();
			itr != _trackmap->end(); ++itr)
		new ((*_tca_trackmap)[i++]) (SvtxTrack_v1)(
				*dynamic_cast<SvtxTrack_v1*>(itr->second));

	i = 0;
	for (SvtxVertexMap::ConstIter itr =
			_vertexmap->begin();
			itr != _vertexmap->end(); ++itr)
		new ((*_tca_vertexmap)[i++]) (SvtxVertex_v1)(
				*dynamic_cast<SvtxVertex_v1*>(itr->second));
	i = 0;
	for (SvtxTrackMap::ConstIter itr =
			_trackmap_refit->begin();
			itr != _trackmap_refit->end(); ++itr)
		new ((*_tca_trackmap_refit)[i++]) (SvtxTrack_v1)(
				*dynamic_cast<SvtxTrack_v1*>(itr->second));

	i = 0;
	for (SvtxVertexMap::ConstIter itr =
			_vertexmap_refit->begin();
			itr != _vertexmap_refit->end(); ++itr)
		new ((*_tca_vertexmap_refit)[i++]) (SvtxVertex_v1)(
				*dynamic_cast<SvtxVertex_v1*>(itr->second));


	_eval_tree->Fill();

	return;
}

/*
 * init_eval_tree
 */
void PHG4TrackKalmanFitter::init_eval_tree()
{
	if(!_tca_particlemap) _tca_particlemap = new TClonesArray("PHG4Particlev2");
	if(!_tca_vtxmap) _tca_vtxmap = new TClonesArray("PHG4VtxPointv1");

	if(!_tca_trackmap) _tca_trackmap = new TClonesArray("SvtxTrack_v1");
	if(!_tca_vertexmap) _tca_vertexmap = new TClonesArray("SvtxVertex_v1");
	if(!_tca_trackmap_refit) _tca_trackmap_refit = new TClonesArray("SvtxTrack_v1");
	if(!_tca_vertexmap_refit) _tca_vertexmap_refit = new TClonesArray("SvtxVertex_v1");


	//! create TTree
	_eval_tree = new TTree("T", "PHG4TrackKalmanFitter Evaluation");

	_eval_tree->Branch("PrimaryParticle", _tca_particlemap);
	_eval_tree->Branch("TruethVtx", _tca_vtxmap);

	_eval_tree->Branch("SvtxTrack", _tca_trackmap);
	_eval_tree->Branch("SvtxVertex", _tca_vertexmap);
	_eval_tree->Branch("SvtxTrackRefit", _tca_trackmap_refit);
	_eval_tree->Branch("SvtxVertexRefit", _tca_vertexmap_refit);

}

/*
 * reset_eval_variables():
 *  Reset all the tree variables to their default values.
 *  Needs to be called at the start of every event
 */
void PHG4TrackKalmanFitter::reset_eval_variables() {
	_tca_particlemap->Clear();
	_tca_vtxmap->Clear();

	_tca_trackmap->Clear();
	_tca_vertexmap->Clear();
	_tca_trackmap_refit->Clear();
	_tca_vertexmap_refit->Clear();
}

int PHG4TrackKalmanFitter::CreateNodes(PHCompositeNode *topNode) {
	// create nodes...
	PHNodeIterator iter(topNode);

	PHCompositeNode *dstNode = static_cast<PHCompositeNode*>(iter.findFirst(
			"PHCompositeNode", "DST"));
	if (!dstNode) {
		cerr << PHWHERE << "DST Node missing, doing nothing." << endl;
		return Fun4AllReturnCodes::ABORTEVENT;
	}

	// Create the SVTX node
	PHCompositeNode* tb_node = dynamic_cast<PHCompositeNode*>(iter.findFirst(
			"PHCompositeNode", "SVTX"));
	if (!tb_node) {
		tb_node = new PHCompositeNode("SVTX");
		dstNode->addNode(tb_node);
		if (verbosity > 0)
			cout << "SVTX node added" << endl;
	}

	_trackmap_refit = new SvtxTrackMap_v1;
	PHIODataNode<PHObject>* tracks_node = new PHIODataNode<PHObject>(
			_trackmap_refit, "SvtxTrackMapRefit", "PHObject");
	tb_node->addNode(tracks_node);
	if (verbosity > 0)
		cout << "Svtx/SvtxTrackMapRefit node added" << endl;

	_vertexmap_refit = new SvtxVertexMap_v1;
	PHIODataNode<PHObject>* vertexes_node = new PHIODataNode<PHObject>(
			_vertexmap_refit, "SvtxVertexMapRefit", "PHObject");
	tb_node->addNode(vertexes_node);
	if (verbosity > 0)
		cout << "Svtx/SvtxVertexMapRefit node added" << endl;

	return Fun4AllReturnCodes::EVENT_OK;
}

/*
 * GetNodes():
 *  Get all the all the required nodes off the node tree
 */
int PHG4TrackKalmanFitter::GetNodes(PHCompositeNode * topNode) {
	//DST objects
	//Truth container
	_truth_container = findNode::getClass<PHG4TruthInfoContainer>(topNode,
			"G4TruthInfo");
	if (!_truth_container && _event < 2) {
		cout << PHWHERE << " PHG4TruthInfoContainer node not found on node tree"
				<< endl;
		return Fun4AllReturnCodes::ABORTEVENT;
	}

	// Input Svtx Clusters
	_clustermap = findNode::getClass<SvtxClusterMap>(topNode, "SvtxClusterMap");
	if (!_clustermap && _event < 2) {
		cout << PHWHERE << " SvtxClusterMap node not found on node tree"
				<< endl;
		return Fun4AllReturnCodes::ABORTEVENT;
	}

	// Input Svtx Tracks
	_trackmap = findNode::getClass<SvtxTrackMap>(topNode, "SvtxTrackMap");
	if (!_trackmap && _event < 2) {
		cout << PHWHERE << " SvtxClusterMap node not found on node tree"
				<< endl;
		return Fun4AllReturnCodes::ABORTEVENT;
	}

	// Input Svtx Vertices
	_vertexmap = findNode::getClass<SvtxVertexMap>(topNode, "SvtxVertexMap");
	if (!_vertexmap && _event < 2) {
		cout << PHWHERE << " SvtxVertexrMap node not found on node tree"
				<< endl;
		return Fun4AllReturnCodes::ABORTEVENT;
	}

	// Output Svtx Tracks
	_trackmap_refit = findNode::getClass<SvtxTrackMap>(topNode,
			"SvtxTrackMapRefit");
	if (!_trackmap_refit && _event < 2) {
		cout << PHWHERE << " SvtxTrackMapRefit node not found on node tree"
				<< endl;
		return Fun4AllReturnCodes::ABORTEVENT;
	}

	// Output Svtx Vertices
	_vertexmap_refit = findNode::getClass<SvtxVertexMap>(topNode,
			"SvtxVertexMapRefit");
	if (!_vertexmap_refit && _event < 2) {
		cout << PHWHERE << " SvtxVertexMapRefit node not found on node tree"
				<< endl;
		return Fun4AllReturnCodes::ABORTEVENT;
	}

	return Fun4AllReturnCodes::EVENT_OK;
}

/*
 * fit track with SvtxTrack as input seed.
 * \param intrack Input SvtxTrack
 */
PHGenFit::Track* PHG4TrackKalmanFitter::ReFitTrack(const SvtxTrack* intrack) {
	if(!intrack){
		cerr << PHWHERE << " Input SvtxTrack is NULL!"
						<< endl;
		return NULL;
	}

	// prepare seed from input SvtxTrack
	TVector3 seed_mom(intrack->get_px(),intrack->get_py(),intrack->get_pz());
	TVector3 seed_pos(intrack->get_x(),intrack->get_y(),intrack->get_z());
	TMatrixDSym seed_cov(6);
	for(int i=0;i<6;i++)
	{
		for(int j=0;j<6;j++)
		{
			seed_cov[i][j] = intrack->get_error(i,j);
		}
	}


	/*!
	 * mu+:	-13
	 * mu-:	13
	 * pi+:	211
	 * pi-:	-211
	 * e-:	11
	 * e+:	-11
	 */
	//TODO Add multiple TrackRep choices.
	int pid = 211;
	genfit::AbsTrackRep* rep = new genfit::RKTrackRep(pid);
	PHGenFit::Track* track = new PHGenFit::Track(rep, seed_pos,
			seed_mom, seed_cov);

	// Create measurements
	std::vector<PHGenFit::Measurement*> measurements;

	for (SvtxTrack::ConstClusterIter iter = intrack->begin_clusters();
			iter != intrack->end_clusters(); ++iter) {
		unsigned int cluster_id = *iter;
		SvtxCluster* cluster = _clustermap->get(cluster_id);
		//unsigned int l = cluster->get_layer();

		TVector3 pos(cluster->get_x(), cluster->get_y(), cluster->get_z());
		TVector3 n(cluster->get_x(), cluster->get_y(), 0);

		//TODO use u, v explicitly?
		PHGenFit::Measurement* meas = new PHGenFit::PlanarMeasurement(pos, n,
				cluster->get_phi_size(), cluster->get_z_size());

		measurements.push_back(meas);
	}

	//TODO unsorted measurements, should use sorted ones?
	track->addMeasurements(measurements);

	//! Fit the track
	_fitter->processTrack(track, false);

	//TODO if not convered, make some noise

	return track;
}

/*
 * Make SvtxTrack from PHGenFit::Track and SvtxTrack
 */
SvtxTrack* PHG4TrackKalmanFitter::MakeSvtxTrack(const SvtxTrack* svtx_track,
		const PHGenFit::Track* phgf_track) {

	double chi2 = phgf_track->get_chi2();
	double ndf = phgf_track->get_ndf();

	genfit::MeasuredStateOnPlane* gf_state = phgf_track->extrapolateToLine(TVector3(0.,0.,0.), TVector3(0.,0.,1.));
	TVector3 mom = gf_state->getMom();
	TVector3 pos = gf_state->getPos();
	TMatrixDSym cov = gf_state->get6DCov();


	//const SvtxTrack_v1* temp_track = static_cast<const SvtxTrack_v1*> (svtx_track);
	SvtxTrack_v1* out_track = new SvtxTrack_v1(*static_cast<const SvtxTrack_v1*> (svtx_track));

	/*!
	 *  1/p, u'/z', v'/z', u, v
	 *  u is defined as mom X beam line at POCA
	 *  so u is the dca2d direction
	 */
	double dca2d = gf_state->getState()[3];
	out_track->set_dca2d(dca2d);
	out_track->set_dca2d_error(gf_state->getCov()[3][3]);
	double dca3d = sqrt(
			dca2d*dca2d +
			gf_state->getState()[4]*gf_state->getState()[4]);
	out_track->set_dca(dca3d);


	out_track->set_chisq(chi2);
	out_track->set_ndf(ndf);
	out_track->set_charge((_reverse_mag_field) ? -1.*phgf_track->get_charge() : phgf_track->get_charge());

	out_track->set_px(mom.Px());
	out_track->set_py(mom.Py());
	out_track->set_pz(mom.Pz());

	out_track->set_x(pos.X());
	out_track->set_y(pos.Y());
	out_track->set_z(pos.Z());

	for(int i=0;i<6;i++)
	{
		for(int j=i;j<6;j++)
		{
			out_track->set_error(i,j,cov[i][j]);
		}
	}

	return out_track;
}

/*
 * Fill SvtxVertexMap from GFRaveVertexes and Tracks
 */
bool PHG4TrackKalmanFitter::FillSvtxVertexMap(
		const std::vector<genfit::GFRaveVertex*>& rave_vertices,
		const std::vector<genfit::Track*>& gf_tracks) {

	for(unsigned int ivtx = 0; ivtx<rave_vertices.size(); ++ivtx) {
		genfit::GFRaveVertex* rave_vtx = rave_vertices[ivtx];

		if(!rave_vtx)
		{
			cerr << PHWHERE << endl;
			return false;
		}

		SvtxVertex* svtx_vtx = new SvtxVertex_v1();

		svtx_vtx->set_chisq(rave_vtx->getChi2());
		svtx_vtx->set_ndof(rave_vtx->getNdf());
		svtx_vtx->set_position(0, rave_vtx->getPos().X());
		svtx_vtx->set_position(1, rave_vtx->getPos().Y());
		svtx_vtx->set_position(2, rave_vtx->getPos().Z());
		for(int i=0;i<3;i++)
			for(int j=0;j<3;j++)
				svtx_vtx->set_error(i,j,rave_vtx->getCov()[i][j]);

		for(unsigned int i=0;i<rave_vtx->getNTracks();i++)
		{
			//TODO Assume id's are sync'ed between _trackmap_refit and gf_tracks, need to change?
			const genfit::Track* rave_track = rave_vtx->getParameters(i)->getTrack();
			for(unsigned int j=0;j<gf_tracks.size();j++)
			{
				if(rave_track == gf_tracks[j])
					svtx_vtx->insert_track(i);
			}
		}

		_vertexmap_refit->insert(svtx_vtx);

		if(verbosity >= 2)
		{
			cout << PHWHERE <<endl;
			svtx_vtx->Print();
			_vertexmap_refit->Print();
		}
	}

	return true;
}
















