/*
 *  \author Julia Yarba
 */
// below edited by adnan cosmic muon  particle gun

#include <ostream>
#include "IOMC/ParticleGuns/src/FlatRandomAngleEGunProducer.h"

#include "SimDataFormats/GeneratorProducts/interface/HepMCProduct.h"
#include "SimDataFormats/GeneratorProducts/interface/GenEventInfoProduct.h"

#include "FWCore/Framework/interface/Event.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "FWCore/ServiceRegistry/interface/Service.h"
#include "FWCore/AbstractServices/interface/RandomNumberGenerator.h"

#include "CLHEP/Random/RandFlat.h"

using namespace edm;
using namespace std;

FlatRandomAngleEGunProducer::FlatRandomAngleEGunProducer(const ParameterSet& pset) 
    : BaseFlatGunProducer(pset) {
  ParameterSet defpset;
  ParameterSet pgun_params = pset.getParameter<ParameterSet>("PGunParameters");

  fMinE = pgun_params.getParameter<double>("MinE");
  fMaxE = pgun_params.getParameter<double>("MaxE");

  fMinTheta = pgun_params.getParameter<double>("MinTheta"); // Cosmic muons
  fMaxTheta = pgun_params.getParameter<double>("MaxTheta");

  fMinPhi = pgun_params.getParameter<double>("MinPhi"); // Flat phi distribution
  fMaxPhi = pgun_params.getParameter<double>("MaxPhi");

  fMinX = pgun_params.getParameter<double>("MinX"); // Flat x distribution
  fMaxX = pgun_params.getParameter<double>("MaxX");

  fMinY = pgun_params.getParameter<double>("MinY"); // Flat y distribution
  fMaxY = pgun_params.getParameter<double>("MaxY");

  zpos_ = pgun_params.getParameter<double>("ZPosition");

  produces<HepMCProduct>("unsmeared");
  produces<GenEventInfoProduct>();

  cout << "Cosmic muon FlatRandomAngleEGun initialized" << endl;
}

FlatRandomAngleEGunProducer::~FlatRandomAngleEGunProducer() {}

void FlatRandomAngleEGunProducer::produce(Event& e, const EventSetup& es) {
  edm::Service<edm::RandomNumberGenerator> rng;
  CLHEP::HepRandomEngine* engine = &rng->getEngine(e.streamID());

  if (fVerbosity > 0) {
    cout << " FlatRandomAngleEGunProducer : Begin New Event Generation" << endl;
  }

  fEvt = new HepMC::GenEvent();
  //double al = CLHEP::RandFlat::shoot(engine, 0, 6.28);     // added with changes by dselivan: angle al(pha) to be varied for a full ring (0-2pi)
 // double x = CLHEP::RandFlat::shoot(engine, fMinX, fMaxX); // dselivan changes (07/2026); this is just not used
  double al = CLHEP::RandFlat::shoot(engine, fMinX, fMaxX);  // extra change on 2026-07-14, so that the angle can be taken from py run file
  double y  = CLHEP::RandFlat::shoot(engine, fMinY, fMaxY);  // for dselivan's changes this is radius

  // Assign coordinates for this event
  HepMC::GenVertex* Vtx = new HepMC::GenVertex(
       //HepMC::FourVector(x * cm2mm_, y * cm2mm_, zpos_ * cm2mm_));
       // changed by dselivan on 2026-07-13 for shooting at specific Tilemodule ring:
       HepMC::FourVector((y * std::cos(al)) * cm2mm_, (y * std::sin(al)) * cm2mm_, zpos_ * cm2mm_));
  std::cerr << y << std::endl;
  int barcode = 1;
  for (unsigned int ip = 0; ip < fPartIDs.size(); ip++) {
    // Generate energy
    double energy = CLHEP::RandFlat::shoot(engine, fMinE, fMaxE);

    // Generate theta using rejection sampling for cos^2(theta)
    double theta;
    bool accepted = false;
    while (!accepted) {
      double draw = CLHEP::RandFlat::shoot(engine, 0.0, 1.0); // Acceptance criterion
      theta = CLHEP::RandFlat::shoot(engine, fMinTheta, fMaxTheta); // Random theta
      double prob = std::pow(std::cos(theta), 2); // Weight: cosÂ²(theta)
      if (prob > draw) {
        accepted = true;
      }
    }

    // Generate phi uniformly
    double phi = CLHEP::RandFlat::shoot(engine, fMinPhi, fMaxPhi);

    // Calculate momentum
    int PartID = fPartIDs[ip];
    const HepPDT::ParticleData* PData = fPDGTable->particle(HepPDT::ParticleID(abs(PartID)));
    double mass = PData->mass().value();
    double mom2 = energy * energy - mass * mass;
    double mom = (mom2 > 0.) ? std::sqrt(mom2) : 0.;

    double px = mom * std::sin(theta) * std::cos(phi);
    double py = mom * std::sin(theta) * std::sin(phi);
    double pz = mom * std::cos(theta);

    // Create particle and add it to the event
    HepMC::FourVector p(px, py, pz, energy);
    HepMC::GenParticle* Part = new HepMC::GenParticle(p, PartID, 1);
    Part->suggest_barcode(barcode);
    barcode++;
    Vtx->add_particle_out(Part);

    // Add antiparticle if required
    if (fAddAntiParticle) {
      HepMC::FourVector ap(-px, -py, -pz, energy);
      int APartID = (PartID == 22 || PartID == 23) ? PartID : -PartID;
      HepMC::GenParticle* APart = new HepMC::GenParticle(ap, APartID, 1);
      APart->suggest_barcode(barcode);
      barcode++;
      Vtx->add_particle_out(APart);
    }
  }

  fEvt->add_vertex(Vtx);
  fEvt->set_event_number(e.id().event());
  fEvt->set_signal_process_id(20);

  if (fVerbosity > 0) {
    fEvt->print();
  }

  unique_ptr<HepMCProduct> BProduct(new HepMCProduct());
  BProduct->addHepMCData(fEvt);
  e.put(std::move(BProduct), "unsmeared");

  unique_ptr<GenEventInfoProduct> genEventInfo(new GenEventInfoProduct(fEvt));
  e.put(std::move(genEventInfo));

  if (fVerbosity > 0) {
    cout << " FlatRandomAngleEGunProducer : Event Generation Done " << endl;
  }
}

#include "FWCore/Framework/interface/MakerMacros.h"
using edm::FlatRandomAngleEGunProducer;
DEFINE_FWK_MODULE(FlatRandomAngleEGunProducer);
