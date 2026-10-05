#include "SimCalorimetry/HGCalSimAlgos/interface/HGCalSciNoiseMap.h"
#include "FWCore/ParameterSet/interface/FileInPath.h"
#include "CLHEP/Random/RandGauss.h"
#include "CLHEP/Random/Random.h"
#include <fstream>
#include <iostream>
#include <random>

//
HGCalSciNoiseMap::HGCalSciNoiseMap()
    : refEdge_(3.),
      ignoreSiPMarea_(false),
      overrideSiPMarea_(false),
      ignoreTileArea_(false),
      ignoreDoseScale_(false),
      ignoreFluenceScale_(false),
      ignoreNoise_(false),
      refDarkCurrent_(0.5),
      aimMIPtoADC_(15),
      maxSiPMPE_(8888) {


// !!! outdated version of the scaling, old LY values, no high density tiles, incorrect assumptions about gain adjustments -----------------------------------------------
// outdated start

  //number of photo electrons per MIP per scintillator type (irradiated, based on testbeam results)
  //reference is a 30*30 mm^2 tile and 2 mm^2 SiPM (with 15um pixels), at the 2 V over-voltage
  //based on https://indico.cern.ch/event/927798/contributions/3900921/attachments/2054679/3444966/2020Jun10_sn_scenes.pdf
  nPEperMIP_[CAST] = 80.31 / 2;     //cast
  nPEperMIP_[MOULDED] = 57.35 / 2;  //moulded

  //full scale charge per gain in nPE
  //this is chosen for now such that the ref. MIP peak is at N ADC counts
  //to be changed once the specs are fully defined such that the algorithm chooses the best gain
  //for the MIP peak to be close to N ADC counts (similar to Si) or applying other specific criteria
  fscADCPerGain_[GAIN_2] = nPEperMIP_[CAST] * 1024. / aimMIPtoADC_;      //2 mm^2  SiPM
  fscADCPerGain_[GAIN_4] = 2 * nPEperMIP_[CAST] * 1024. / aimMIPtoADC_;  //4mm^2   SiPM
  

  //lsb: adc has 10 bits -> 1024 counts at max
  for (size_t i = 0; i < GAINRANGE_N; i++)
    lsbPerGain_[i] = fscADCPerGain_[i] / 1024.f;
}

// unused in updated digitiser
void HGCalSciNoiseMap::setDoseMap(const std::string& fullpath, const unsigned int algo) {
  //decode bits of the algo word
  ignoreSiPMarea_ = ((algo >> IGNORE_SIPMAREA) & 0x1);
  overrideSiPMarea_ = ((algo >> OVERRIDE_SIPMAREA) & 0x1);
  ignoreTileArea_ = ((algo >> IGNORE_TILEAREA) & 0x1);
  ignoreDoseScale_ = ((algo >> IGNORE_DOSESCALE) & 0x1);
  ignoreFluenceScale_ = ((algo >> IGNORE_FLUENCESCALE) & 0x1);
  ignoreNoise_ = ((algo >> IGNORE_NOISE) & 0x1);
  ignoreTileType_ = ((algo >> IGNORE_TILETYPE) & 0x1);

  //call base class method
  HGCalRadiationMap::setDoseMap(fullpath, algo);
}

//
// unused in updated digitiser
void HGCalSciNoiseMap::setSipmMap(const std::string& fullpath) { sipmMap_ = readSipmPars(fullpath); }

//
// unused in updated digitiser
void HGCalSciNoiseMap::setNpePerMIP(float npePerMIP) {
  nPEperMIP_[CAST] = npePerMIP;
  nPEperMIP_[MOULDED] = npePerMIP;
}

//
// unused in updated digitiser
std::unordered_map<int, float> HGCalSciNoiseMap::readSipmPars(const std::string& fullpath) {
  std::unordered_map<int, float> result;
  //no file means default sipm size
  if (fullpath.empty())
    return result;

  edm::FileInPath fp(fullpath);
  std::ifstream infile(fp.fullPath());
  if (!infile.is_open()) {
    throw cms::Exception("FileNotFound") << "Unable to open '" << fullpath << "'" << std::endl;
  }
  std::string line;
  while (getline(infile, line)) {
    int layer;
    float boundary;

    //space-separated
    std::stringstream linestream(line);
    linestream >> layer >> boundary;

    result[layer] = boundary;
  }
  return result;
}

//
// unused in updated digitiser
void HGCalSciNoiseMap::setReferenceDarkCurrent(double idark) { refDarkCurrent_ = idark; }



HGCalSciNoiseMap::SiPMonTileCharacteristics HGCalSciNoiseMap::scaleByDose(const HGCScintillatorDetId& cellId,
                                                                          const double radius,
                                                                          int aimMIPtoADC,
                                                                          GainRange_t gainPreChoice) {
  int layer = cellId.layer();
  bool hasDoseMap(!(getDoseMap().empty()));

  //LIGHT YIELD
  double lyScaleFactor(1.f);
  //formula is: A = A0 * exp( -D^0.65 / 199.6)
  //where A0 is the response of the undamaged detector, D is the dose
  if (!ignoreDoseScale_ && hasDoseMap) {
    double cellDose = getDoseValue(DetId::HGCalHSc, layer, radius);  //in kRad
    constexpr double expofactor = 1. / 199.6;
    const double dosespower = 0.65;
    lyScaleFactor = std::exp(-std::pow(cellDose, dosespower) * expofactor);
  }

  //NOISE
  double noise(0.f);
  if (!ignoreNoise_) {
    double cellFluence = getFluenceValue(DetId::HGCalHSc, layer, radius);  //in 1-Mev-equivalent neutrons per cm2

    //MODEL 1 : formula is N = 2.18 * sqrt(F * A / 2e13)
    //where F is the fluence and A is the SiPM area (scaling with the latter is done below)
    if (refDarkCurrent_ < 0) {
      noise = 2.18;
      if (!ignoreFluenceScale_ && hasDoseMap) {
        constexpr double fluencefactor = 2. / (2 * 1e13);  //reference SiPM area = 2mm^2
        noise *= sqrt(cellFluence * fluencefactor);
      }
    }

    //MODEL 2 : formula is  3.16 *  sqrt( (Idark * 1e-12) / (qe * gain) * (F / F0) )
    //where F is the fluence (neq/cm2), gain is the SiPM gain, qe is the electron charge (C), Idark is dark current (mA)
    else {
      constexpr double refFluence(2.0E+13);
      constexpr double refGain(235000.);
      double Rdark = (refDarkCurrent_ * 1E-12) / (CLHEP::e_SI * refGain);
      if (!ignoreFluenceScale_ && hasDoseMap)
        Rdark *= (cellFluence / refFluence);
      noise = 3.16 * sqrt(Rdark);
    }
  }

  //ADDITIONAL SCALING FACTORS
  double tileAreaSF = scaleByTileArea(cellId, radius);
  std::pair<double, HGCalSciNoiseMap::GainRange_t> sipm = scaleBySipmArea(cellId, radius, gainPreChoice);
  double sipmAreaSF = sipm.first;
  HGCalSciNoiseMap::GainRange_t gain = sipm.second;

  lyScaleFactor *= tileAreaSF * sipmAreaSF;
  noise *= sqrt(sipmAreaSF);

  //final signal depending on scintillator type
  double S(nPEperMIP_[CAST]);
  if (!ignoreTileType_ && cellId.type() == 2)
    S = nPEperMIP_[MOULDED];
  S *= lyScaleFactor;

  HGCalSciNoiseMap::SiPMonTileCharacteristics sipmChar;
  sipmChar.s = S;
  sipmChar.lySF = lyScaleFactor;
  sipmChar.n = noise;
  sipmChar.gain = gain;
  sipmChar.thrADC = std::floor(0.5 * S / lsbPerGain_[gain]);
  sipmChar.ntotalPE = maxSiPMPE_ * sipmAreaSF;
  sipmChar.xtalk = refXtalk_;
  return sipmChar;
}

double HGCalSciNoiseMap::scaleByTileArea(const HGCScintillatorDetId& cellId, const double radius) {
  double scaleFactor(1.f);

  if (ignoreTileArea_)
    return scaleFactor;

  [[clang::suppress]]
  double edge(refEdge_);  //start with reference 3cm of edge
  if (cellId.type() == 0) {
    constexpr double factor = 2 * M_PI * 1. / 360.;
    edge = radius * factor;  //1 degree
  } else {
    constexpr double factor = 2 * M_PI * 1. / 288.;
    edge = radius * factor;  //1.25 degrees
  }
  scaleFactor = refEdge_ / edge;
  return scaleFactor;
}
// outdated end
// -------------------------------------------------------------------------------------------------------------------------------------------------------------- !!!



// Updated digitiser functions (from 09/2026) *******************************************************************************************************************
// updated start

HGCalSciNoiseMap::SiPMonTileCharacteristics HGCalSciNoiseMap::scaleByDose_update(const HGCScintillatorDetId& cellId,
                                                                          const double radius,
                                                                          const double rawID,
                                                                          const double LYsigma,
                                                                          int aimMIPtoADC, // unused in the update
                                                                          GainRange_t gainPreChoice) {
  int layer = cellId.layer();
  bool hasDoseMap(!(getDoseMap().empty()));

  // copied over, but not used and not updated
  //**

  //LIGHT YIELD
  double lyScaleFactor(1.f);
  //formula is: A = A0 * exp( -D^0.65 / 199.6)
  //where A0 is the response of the undamaged detector, D is the dose
  if (!ignoreDoseScale_ && hasDoseMap) {
    double cellDose = getDoseValue(DetId::HGCalHSc, layer, radius);  //in kRad
    constexpr double expofactor = 1. / 199.6;
    const double dosespower = 0.65;
    lyScaleFactor = std::exp(-std::pow(cellDose, dosespower) * expofactor);
  }

  //NOISE
  double noise(0.f);
  if (!ignoreNoise_) {
    double cellFluence = getFluenceValue(DetId::HGCalHSc, layer, radius);  //in 1-Mev-equivalent neutrons per cm2

    //MODEL 1 : formula is N = 2.18 * sqrt(F * A / 2e13)
    //where F is the fluence and A is the SiPM area (scaling with the latter is done below)
    if (refDarkCurrent_ < 0) {
      noise = 2.18;
      if (!ignoreFluenceScale_ && hasDoseMap) {
        constexpr double fluencefactor = 2. / (2 * 1e13);  //reference SiPM area = 2mm^2
        noise *= sqrt(cellFluence * fluencefactor);
      }
    }

    //MODEL 2 : formula is  3.16 *  sqrt( (Idark * 1e-12) / (qe * gain) * (F / F0) )
    //where F is the fluence (neq/cm2), gain is the SiPM gain, qe is the electron charge (C), Idark is dark current (mA)
    else {
      constexpr double refFluence(2.0E+13);
      constexpr double refGain(235000.);
      double Rdark = (refDarkCurrent_ * 1E-12) / (CLHEP::e_SI * refGain);
      if (!ignoreFluenceScale_ && hasDoseMap)
        Rdark *= (cellFluence / refFluence);
      noise = 3.16 * sqrt(Rdark);
    }
  }

  //ADDITIONAL SCALING FACTORS
  double tileAreaSF = scaleByTileArea(cellId, radius);
  std::pair<double, HGCalSciNoiseMap::GainRange_t> sipm = scaleBySipmArea(cellId, radius, gainPreChoice);
  double sipmAreaSF = sipm.first;
  HGCalSciNoiseMap::GainRange_t gain = sipm.second;

  lyScaleFactor *= tileAreaSF * sipmAreaSF;
  noise *= sqrt(sipmAreaSF);

  //final signal depending on scintillator type
  double S(nPEperMIP_[CAST]);
  if (!ignoreTileType_ && cellId.type() == 2)
    S = nPEperMIP_[MOULDED];
  S *= lyScaleFactor;
  //**


  // updated variables
  // -_-_-_-_-_-_-
  double tileAreaLY = LYByTileArea(cellId, radius, rawID, LYsigma);
  double L(-99.0); // something obviously wrong for debugging purposes
  L = tileAreaLY;
  // -_-_-_-_-_-_-

  HGCalSciNoiseMap::SiPMonTileCharacteristics sipmChar;
  sipmChar.s = S; //unused in the digitiser
  sipmChar.lySF = lyScaleFactor; //unused in the digitiser
  sipmChar.n = noise; //unused in the digitiser
  sipmChar.gain = gain; //unused in the digitiser
  sipmChar.thrADC = std::floor(0.5 * S / lsbPerGain_[gain]); //unused in the digitiser, set elsewhere
  sipmChar.ntotalPE = maxSiPMPE_ * sipmAreaSF; //unused in the digitiser
  sipmChar.xtalk = refXtalk_; //unused in the digitiser
  sipmChar.L = L; // updated part, used in the digitiser
  return sipmChar;
}

// new function in the update
// getting tile light yield based on its edge length, LY v edge as formulas, parametrisation from production measurements in DESY's LY test stand

double HGCalSciNoiseMap::LYByTileArea(const HGCScintillatorDetId& cellId, const double radius, const long long int rawID, const double sigma) {
  double ly(80.);

  //if (ignoreTileArea_)
  //  return ly;

  [[clang::suppress]]
  double edge(refEdge_);  //start with reference 3cm of edge

  CLHEP::HepRandom::setTheSeed(rawID); // seeded RNG with tile id as the seed, to be used when a mean value for LY for a given tile size is provided;
				       // this allows to generate a unique LY for all tiles based on measured mean LY and standard deviation, constant for all events
  double var = CLHEP::RandGauss::shoot(1,sigma);

  // Calculating tile edge based on its position (radius) and the granularity
  if (cellId.granularity() == 1) // High density tiles
  	{
  	constexpr double factor = 2 * M_PI * 1. / 432.; // ~0.83 degrees
  	edge = 10*(radius * factor);
  	}
  else
  	{
  	constexpr double factor = 2 * M_PI * 1. / 288.; // 1.25 degrees
  	edge = 10*(radius * factor);
  	}

  // using the calculated tile edge to calculate the mean value for the light yield based on parametrisation for various materials
  // Formulas for 9 mm^2 SiPMs, and SiPM overvoltage of 2V
  // TODO: Configurable functions to be added
  if (cellId.type() == 2) // Molded scintillator tiles
  	{
  	ly = ((4649.7/edge) - 17.1) * ((-0.72*4 + 10.0*2 + 4.41)/(-0.72*16 + 10.0*4 + 4.41)) ; // formula for molded LY for 9mm2 SiPM 2V OV
  	}
  else // all other tiles are supposed to be Cast
  	{
  	ly = ((3101.7/edge) + 72.76) * ((-0.72*4 + 10.0*2 + 4.41)/(-0.72*16 + 10.0*4 + 4.41)) ; // formula for cast LY for 9mm2 SiPM 2V OV
  	}
  // returning the mean value for a given tile size multiplied by the random modifier dependant on the given tile id = LY of that specific tile
  return ly * var;
}
// updated end
// ********************************************************************************************************************************************************************


// !!! outdated functions, unused in updated digitiser, not checked for correctness -----------------------------------------------------------------------------------
// outdated start
std::pair<double, HGCalSciNoiseMap::GainRange_t> HGCalSciNoiseMap::scaleBySipmArea(
    const HGCScintillatorDetId& cellId, const double radius, const HGCalSciNoiseMap::GainRange_t& gainPreChoice) {
  //start with the prechosen gain
  //if auto then override it according to the SiPM area
  HGCalSciNoiseMap::GainRange_t gain(gainPreChoice);
  if (gainPreChoice == HGCalSciNoiseMap::GainRange_t::AUTO)
    gain = GainRange_t::GAIN_2;

  double scaleFactor(1.f);

  if (ignoreSiPMarea_)
    return std::pair<double, HGCalSciNoiseMap::GainRange_t>(scaleFactor, gain);

  //use sipm area boundary map
  if (overrideSiPMarea_) {
    int layer = cellId.layer();
    if (sipmMap_.count(layer) > 0 && radius < sipmMap_[layer]) {
      scaleFactor = 2.f;
      if (gainPreChoice == HGCalSciNoiseMap::GainRange_t::AUTO)
        gain = GainRange_t::GAIN_4;
    }
  }
  //read from DetId
  else {
    int sipm = cellId.sipm();
    if (sipm == 0) {
      scaleFactor = 2.f;
      if (gainPreChoice == HGCalSciNoiseMap::GainRange_t::AUTO)
        gain = GainRange_t::GAIN_4;
    }
  }

  return std::pair<double, HGCalSciNoiseMap::GainRange_t>(scaleFactor, gain);
}
// outdated end
// ---------------------------------------------------------------------------------------------------------------------------------------------------------------------
