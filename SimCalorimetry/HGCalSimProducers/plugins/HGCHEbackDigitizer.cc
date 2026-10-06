#include "SimCalorimetry/HGCalSimProducers/interface/HGCDigitizerBase.h"
#include "DataFormats/HGCDigi/interface/HGCDigiCollections.h"
#include "DataFormats/ForwardDetId/interface/HGCScintillatorDetId.h"
#include "SimCalorimetry/HGCalSimAlgos/interface/HGCalSciNoiseMap.h"
#include "Geometry/HGCalGeometry/interface/HGCalGeometry.h"
#include "SimCalorimetry/HGCalSimProducers/interface/HGCDigitizerPluginFactory.h"

#include "CLHEP/Random/RandPoissonQ.h"
#include "CLHEP/Random/RandGaussQ.h"
#include "CLHEP/Random/RandBinomial.h"
#include "vdt/vdtMath.h"

#include "FWCore/ServiceRegistry/interface/Service.h"
#include "CommonTools/UtilAlgos/interface/TFileService.h"
#include "TH1F.h"
#include "TF1.h"
#include "TH1.h"
#include "TH2.h"

#include <fstream>
#include <iostream>



using namespace hgc_digi;
using namespace hgc_digi_utils;

class HGCHEbackDigitizer : public HGCDigitizerBase {
public:
  HGCHEbackDigitizer(const edm::ParameterSet& ps);
  void runDigitizer(std::unique_ptr<HGCalDigiCollection>& digiColl,
                    hgc::HGCSimHitDataAccumulator& simData,
                    const CaloSubdetectorGeometry* theGeom,
                    const std::unordered_set<DetId>& validIds,
                    CLHEP::HepRandomEngine* engine) override;
  ~HGCHEbackDigitizer() override;

private:
  //calice-like digitization parameters
  uint32_t algo_;
  bool scaleByDose_, thresholdFollowsMIP_;
  float keV2MIP_, noise_MIP_, lightYield_id, gainValue, nTotalPx, maxADC;
  float nPEperMIP_, nTotalPE_, xTalk_, sdPixels_;
  std::string doseMapFile_, sipmMapFile_;
  HGCalSciNoiseMap scal_;
  
  /*
  TH1F *adcs;
  TH1F *posx;
  TH1F *posy;
  TH1F *posz;
  TH1F *ly;
  //edm::Service<TFileService> fs;
  */

  void runEmptyDigitizer(std::unique_ptr<HGCalDigiCollection>& digiColl,
                         hgc::HGCSimHitDataAccumulator& simData,
                         const CaloSubdetectorGeometry* theGeom,
                         const std::unordered_set<DetId>& validIds,
                         CLHEP::HepRandomEngine* engine);

  void runRealisticDigitizer(std::unique_ptr<HGCalDigiCollection>& digiColl,
                             hgc::HGCSimHitDataAccumulator& simData,
                             const CaloSubdetectorGeometry* theGeom,
                             const std::unordered_set<DetId>& validIds,
                             CLHEP::HepRandomEngine* engine);

// Update of the digitizer from 09/2026
// *-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*
  void runRealisticSciDigitizer(std::unique_ptr<HGCalDigiCollection>& digiColl,
                             hgc::HGCSimHitDataAccumulator& simData,
                             const CaloSubdetectorGeometry* theGeom,
                             const std::unordered_set<DetId>& validIds,
                             CLHEP::HepRandomEngine* engine);
// *-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*

  void runCaliceLikeDigitizer(std::unique_ptr<HGCalDigiCollection>& digiColl,
                              hgc::HGCSimHitDataAccumulator& simData,
                              const CaloSubdetectorGeometry* theGeom,
                              const std::unordered_set<DetId>& validIds,
                              CLHEP::HepRandomEngine* engine);

// original attempt at an updaste of the digitiser, but incomplete
// TODO: remove
// -----------------------------------------------------

  void runProperCaliceDigitizer(std::unique_ptr<HGCalDigiCollection>& digiColl,
                              hgc::HGCSimHitDataAccumulator& simData,
                              const CaloSubdetectorGeometry* theGeom,
                              const std::unordered_set<DetId>& validIds,
                              CLHEP::HepRandomEngine* engine);
};
// -----------------------------------------------------



HGCHEbackDigitizer::HGCHEbackDigitizer(const edm::ParameterSet& ps) : HGCDigitizerBase(ps) {
  edm::ParameterSet cfg = ps.getParameter<edm::ParameterSet>("digiCfg");
  algo_ = cfg.getParameter<uint32_t>("algo");
  sipmMapFile_ = cfg.getParameter<edm::ParameterSet>("noise").getParameter<std::string>("sipmMap");
  scaleByDose_ = cfg.getParameter<edm::ParameterSet>("noise").getParameter<bool>("scaleByDose");
  unsigned int scaleByDoseAlgo = cfg.getParameter<edm::ParameterSet>("noise").getParameter<uint32_t>("scaleByDoseAlgo");
  scaleByDoseFactor_ = cfg.getParameter<edm::ParameterSet>("noise").getParameter<double>("scaleByDoseFactor");
  doseMapFile_ = cfg.getParameter<edm::ParameterSet>("noise").getParameter<std::string>("doseMap");
  noise_MIP_ = cfg.getParameter<edm::ParameterSet>("noise").getParameter<double>("noise_MIP");
  double refIdark = cfg.getParameter<edm::ParameterSet>("noise").getParameter<double>("referenceIdark");
  xTalk_ = cfg.getParameter<edm::ParameterSet>("noise").getParameter<double>("referenceXtalk");
  thresholdFollowsMIP_ = cfg.getParameter<bool>("thresholdFollowsMIP");
  keV2MIP_ = cfg.getParameter<double>("keV2MIP");
  this->keV2fC_ = 1.0;  //keV2MIP_; // hack for HEB
  this->det_ = DetId::HGCalHSc;
  nPEperMIP_ = cfg.getParameter<double>("nPEperMIP");
  nTotalPE_ = cfg.getParameter<double>("nTotalPE");
  nTotalPx = cfg.getParameter<double>("nTotalPX");			    ///// added proper number of SiPM pixels for 9 mm^2 SiPM
  lightYield_id = cfg.getParameter<double>("nPEperMIP");                    ///// kept, but unused
  gainValue = cfg.getParameter<double>("SiPM9mmgain4OV12CG");               ///// Constant SiPM gain in units of ADC for the calculation of digis for ConvGain 2 (!) and OverVoltage 4V
									    ///// !!! NAME ALREAD WRONG: the value provided (0.7) is for ConvGain 2, not 12 !!
									    ///// Name should be changed (TODO), can be configured in python, depending on the desired CG and OV
  sdPixels_ = cfg.getParameter<double>("sdPixels"); 			    ///// Unused as sdPixels, but configured still to mean the spread for tile light yield (sigma)
									    ///// for the realistic Sci digitiser, TODO: rename/add new parameter to configure and not confuse!
  maxADC = cfg.getParameter<double>("maxADC_");                             ///// 10-bit adc, max = 1024


// set here, but unused in the updated digitiser for now. Have not been checked!
// ----------------------
  scal_.setDoseMap(doseMapFile_, scaleByDoseAlgo);
  scal_.setReferenceDarkCurrent(refIdark);
  scal_.setFluenceScaleFactor(scaleByDoseFactor_);
  scal_.setSipmMap(sipmMapFile_);
  scal_.setReferenceCrossTalk(xTalk_);

  //the ADC will be updated on the fly depending on the gain                  // Comment related to updated digitiser: this is an incorrect assumption, gain will be constant,
									      // ADC simply recorded, channel equalisation done offline at reconstruction
  //but the TDC scale needs to be updated to use pe instead of MIP units      // TDC still to be properly implemented for the updated digitiser (TODO)
  if (scaleByDose_)
    this->myFEelectronics_->setTDCfsc(2 * scal_.getNPeInSiPM());
}
// ----------------------


//
void HGCHEbackDigitizer::runDigitizer(std::unique_ptr<HGCalDigiCollection>& digiColl,
                                      HGCSimHitDataAccumulator& simData,
                                      const CaloSubdetectorGeometry* theGeom,
                                      const std::unordered_set<DetId>& validIds,
                                      CLHEP::HepRandomEngine* engine) {
  if (algo_ == 0)
    runEmptyDigitizer(digiColl, simData, theGeom, validIds, engine);
  else if (algo_ == 1)
    runCaliceLikeDigitizer(digiColl, simData, theGeom, validIds, engine);
  else if (algo_ == 2)
    runRealisticDigitizer(digiColl, simData, theGeom, validIds, engine);
  else if (algo_ == 3)
    runRealisticSciDigitizer(digiColl, simData, theGeom, validIds, engine);  // updated digitiser as of 09/2026
    //runProperCaliceDigitizer(digiColl, simData, theGeom, validIds, engine);  // TODO: remove completely, unnecessary
}

void HGCHEbackDigitizer::runEmptyDigitizer(std::unique_ptr<HGCalDigiCollection>& digiColl,
                                           HGCSimHitDataAccumulator& simData,
                                           const CaloSubdetectorGeometry* theGeom,
                                           const std::unordered_set<DetId>& validIds,
                                           CLHEP::HepRandomEngine* engine) {
  HGCSimHitData chargeColl, toa;
  // this represents a cell with no signal charge
  HGCCellInfo zeroData;
  zeroData.hit_info[0].fill(0.f);  //accumulated energy
  zeroData.hit_info[1].fill(0.f);  //time-of-flight

  for (const auto& id : validIds) {
    chargeColl.fill(0.f);
    toa.fill(0.f);
    HGCSimHitDataAccumulator::iterator it = simData.find(id);
    HGCCellInfo& cell = (simData.end() == it ? zeroData : it->second);
    addCellMetadata(cell, theGeom, id);

    for (size_t i = 0; i < cell.hit_info[0].size(); ++i) {
      //convert total energy keV->MIP, since converted to keV in accumulator
      const float totalIniMIPs(cell.hit_info[0][i] * keV2MIP_);

      //store
      chargeColl[i] = totalIniMIPs;
    }

    //init a new data frame and run shaper
    HGCalDataFrame newDataFrame(id);
    this->myFEelectronics_->runShaper(newDataFrame, chargeColl, toa, engine);

    //prepare the output
    this->updateOutput(digiColl, newDataFrame);
  }
}

void HGCHEbackDigitizer::runRealisticDigitizer(std::unique_ptr<HGCalDigiCollection>& digiColl,
                                               HGCSimHitDataAccumulator& simData,
                                               const CaloSubdetectorGeometry* theGeom,
                                               const std::unordered_set<DetId>& validIds,
                                               CLHEP::HepRandomEngine* engine) {
  //switch to true if you want to print some details
  constexpr bool debug(false);

  HGCSimHitData chargeColl, toa;
  // this represents a cell with no signal charge
  HGCCellInfo zeroData;
  zeroData.hit_info[0].fill(0.f);  //accumulated energy
  zeroData.hit_info[1].fill(0.f);  //time-of-flight

  // needed to compute the radiation and geometry scale factors
  scal_.setGeometry(theGeom);

  //vanilla reference values are indepenent of the ids and were set by
  //configuration in the python - no need to recomput them every time
  //in the digitization loop
  float scaledPePerMip = nPEperMIP_;                                //needed to scale according to tile geometry
  float tunedNoise = nPEperMIP_ * noise_MIP_;                       //flat noise case
  float vanillaADCThr = this->myFEelectronics_->getADCThreshold();  //vanilla thrs  in MIPs
  float adcLsb(this->myFEelectronics_->getADClsb());
  float maxADC(-1);  //vanilla will rely on what has been configured by default
  uint32_t thrADC(thresholdFollowsMIP_ ? std::floor(vanillaADCThr / adcLsb * scaledPePerMip / nPEperMIP_)
                                       : std::floor(vanillaADCThr / adcLsb));
  float nTotalPixels(nTotalPE_);
  float xTalk(xTalk_);
  int gainIdx(0);

  for (const auto& id : validIds) {
    chargeColl.fill(0.f);
    toa.fill(0.f);
    HGCSimHitDataAccumulator::iterator it = simData.find(id);
    HGCCellInfo& cell = (simData.end() == it ? zeroData : it->second);
    addCellMetadata(cell, theGeom, id);

    //in case realistic scenario (noise, fluence, dose, sipm/tile area) are to be used
    //we update vanilla values with the realistic ones
    if (id.det() == DetId::HGCalHSc && scaleByDose_) {
      float sigma = 0.05;
      HGCScintillatorDetId scId(id.rawId());
      double radius = scal_.computeRadius(scId);
      auto opChar = scal_.scaleByDose(scId, radius);
      scaledPePerMip = opChar.s;
      tunedNoise = opChar.n;
      gainIdx = opChar.gain;
      thrADC = opChar.thrADC;
      adcLsb = scal_.getLSBPerGain()[gainIdx];
      maxADC = scal_.getMaxADCPerGain()[gainIdx] - 1e-6;
      nTotalPixels = opChar.ntotalPE;
      xTalk = opChar.xtalk;
    }

    //set mean for poissonian noise
    float meanN = std::pow(tunedNoise, 2);

    for (size_t i = 0; i < cell.hit_info[0].size(); ++i) {
      //convert total energy keV->MIP, since converted to keV in accumulator
      float totalIniMIPs(cell.hit_info[0][i] * keV2MIP_);

      //generate the number of photo-electrons from the energy deposit
      const uint32_t npeS = std::floor(CLHEP::RandPoissonQ::shoot(engine, totalIniMIPs * scaledPePerMip) + 0.5);

      //generate the noise associated to the dark current
      const uint32_t npeN = std::floor(CLHEP::RandPoissonQ::shoot(engine, meanN) + 0.5);

      //total number of pe from signal + noise  (not subtracting pedestal)
      const uint32_t npe = npeS + npeN;

      //take into account SiPM saturation
      uint32_t nPixel(npe);
      if (xTalk >= 0) {
        const float x = vdt::fast_expf(-((float)npe) / nTotalPixels);
        if (xTalk_ * x != 1)
          nPixel = (uint32_t)std::max(nTotalPixels * (1.f - x) / (1.f - xTalk_ * x), 0.f);
      }

      //take into account the gain fluctuations of each pixel
      //FDG: just a note for now, par to be defined
      //const float nPixelTot = nPixel + sqrt(nPixel) * CLHEP::RandGaussQ::shoot(engine, 0., 0.05);

      //realistic behavior: subtract the pedestal
      //Note: for now the saturation effects are ignored...
      if (scaleByDose_) {
        float pedestal(meanN);
        if (scal_.ignoreAutoPedestalSubtraction())
          pedestal = 0.f;
        chargeColl[i] = std::max(nPixel - pedestal, 0.f);
      }
      //vanilla simulation: scale back to MIP units... and to calibrated response depending on the thresholdFollowsMIP_ flag
      else {
        float totalMIPs = thresholdFollowsMIP_ ? std::max((nPixel - meanN), 0.f) / nPEperMIP_ : nPixel / nPEperMIP_;

        if (debug && totalIniMIPs > 0) {
          LogDebug("HGCHEbackDigitizer") << "npeS: " << npeS << " npeN: " << npeN << " npe: " << npe
                                         << " meanN: " << meanN << " noise_MIP_: " << noise_MIP_
                                         << " nPEperMIP_: " << nPEperMIP_ << " scaledPePerMip: " << scaledPePerMip
                                         << " nPixel: " << nPixel;
          LogDebug("HGCHEbackDigitizer") << "totalIniMIPs: " << totalIniMIPs << " totalMIPs: " << totalMIPs
                                         << std::endl;
        }

        //store charge
        chargeColl[i] = totalMIPs;
      }

      //update time of arrival
      toa[i] = cell.hit_info[1][i];
      if (myFEelectronics_->toaMode() == HGCFEElectronics<HGCalDataFrame>::WEIGHTEDBYE && totalIniMIPs > 0)
        toa[i] = cell.hit_info[1][i] / totalIniMIPs;
    }

    //init a new data frame and run shaper
    HGCalDataFrame newDataFrame(id);
    this->myFEelectronics_->runShaper(newDataFrame, chargeColl, toa, engine, thrADC, adcLsb, gainIdx, maxADC);

    //prepare the output
    this->updateOutput(digiColl, newDataFrame);
  }
}



void HGCHEbackDigitizer::runRealisticSciDigitizer(std::unique_ptr<HGCalDigiCollection>& digiColl,
                                               HGCSimHitDataAccumulator& simData,
                                               const CaloSubdetectorGeometry* theGeom,
                                               const std::unordered_set<DetId>& validIds,
                                               CLHEP::HepRandomEngine* engine) {
  //switch to true if you want to print some details
  constexpr bool debug(false);
  constexpr bool doSaturation(true);    // flag to perform emulation of saturation of SiPM pixels
  constexpr bool doSmearing(true);      // flag to perform emulation of statistical fluctiations of n of fired pixels

  HGCSimHitData chargeColl, toa;
  // this represents a cell with no signal charge
  HGCCellInfo zeroData;
  zeroData.hit_info[0].fill(0.f);  //accumulated energy
  zeroData.hit_info[1].fill(0.f);  //time-of-flight

  // needed to compute the radiation and geometry scale factors
  scal_.setGeometry(theGeom);   // unused at the moment, untested (10/2026)

  // vanilla reference values are indepenent of the ids and were set by   // UPD: outdated
  // configuration in the python - no need to recomput them every time    // UPD: outdated
  // in the digitization loop // UPD: outdated
  //float scaledPePerMip = nPEperMIP_;                                // needed to scale according to tile geometry  // UPD: outdated
  float scaledLYPerMip = -98.8; 				    // UPD: set to obviously wrong for debugging purposes
  float tunedNoise = nPEperMIP_ * noise_MIP_;                       // flat noise case // UPD: outdated/unused at the mement (10/2026)
  float vanillaADCThr = this->myFEelectronics_->getADCThreshold();  // vanilla thrs in MIPs 

  float adcLsb(1); // UPD: Set to 1 to pass the ADC value to the shaper to form digis (not charge)
  float maxADC(-1);  //vanilla will rely on what has been configured by default

  //uint32_t thrADC(thresholdFollowsMIP_ ? std::floor(vanillaADCThr / adcLsb * scaledPePerMip / nPEperMIP_)     //changed by dselivan on 2026-04-17
  //                                     : std::floor(vanillaADCThr / adcLsb));

  // UPD: threshold calcuilated based on the expected ADC for ope MIP * the vanilla threshold in MIPs:
  uint32_t thrADC(thresholdFollowsMIP_ ? std::floor(vanillaADCThr * scaledLYPerMip * gainValue)
                                       : std::floor(vanillaADCThr / adcLsb)); // UPD: TODO check if this ever happens, could be wrong for the updated digitisation
  //float nTotalPixels(nTotalPE_);
  //float xTalk(xTalk_);
  int gainIdx(0);  // UPD: unused, incorrect assumption about channel equalisation

  for (const auto& id : validIds) {
    chargeColl.fill(0.f);
    toa.fill(0.f);
    HGCSimHitDataAccumulator::iterator it = simData.find(id);
    HGCCellInfo& cell = (simData.end() == it ? zeroData : it->second);
    addCellMetadata(cell, theGeom, id);
    float simHitADC(0.f);
    int nPixels_sat = -99;
    int nPixels_sat_smear = -88;
    double radius; //, x, y, z;
    //double tileE, tileA;
    int pedestal = 150; // UPD: value of the expected position of the pedestal determined for the runs
    float adctotfact = 3.; 

    float sigma = sdPixels_; // --- SD PIXELS used for LY sigma setup from the run cfg.py since it's not used otherwise, to not add new parameters; TODO: add new, to not confuse

    //in case realistic scenario (noise, fluence, dose, sipm/tile area) are to be used
    //we update vanilla values with the realistic ones
    //if (id.det() == DetId::HGCalHSc) {  //&& scaleByDose_) { // UPD: already true, since this digitiser is only used for the scintillator portion of the HGCAL
      HGCScintillatorDetId scId(id.rawId());
      radius = scal_.computeRadius(scId);
      auto opChar = scal_.scaleByDose_update(scId, radius, id.rawId(), sigma); // using new for digitiser update
      auto opTileA = scal_.scaleByTileArea(scId, radius); // unused
      auto opPos = scal_.computePos(scId);
      auto opA = scal_.computeArea(scId);
      x = opPos.x();
      y = opPos.y();
      z = opPos.z();
      tileA = opA;
      tileE = opTileA;
      scaledPePerMip = opChar.s;
      scaledLYPerMip = opChar.L; // the only actual value used for the updated digitiser (TODO: check other values for correctness and update or remove!)
      tunedNoise = opChar.n;
      gainIdx = opChar.gain;
      thrADC = std::floor(vanillaADCThr * scaledLYPerMip);
      adcLsb = 1;
      maxADC = 1024 - pedestal; // UPD: max ADC not 1024, since some of it will be taken by the pedestal, recalculating.

    //} // ----------------------------------------------------------------------------------------------------------------------------------

    //set mean for poissonian noise // UPD: untested, unused at the moment (10/2026)
    float meanN = std::pow(tunedNoise, 2);

    for (size_t i = 0; i < cell.hit_info[0].size(); ++i) {
      //convert total energy keV->MIP, since converted to keV in accumulator
      float totalIniMIPs(cell.hit_info[0][i] * keV2MIP_);

      // generate the mean number of photo-electrons from the energy deposit
      int nPixels =  std::floor(totalIniMIPs * scaledLYPerMip);
      // apply SiPM saturation function
      if(doSaturation){
        nPixels_sat = std::floor(nTotalPx * (1 - vdt::fast_expf(-nPixels/ nTotalPx)));
        // Binomial smearing to emulate statistical fluctuations for fired pixels
	if(doSmearing){
             float prob = nPixels_sat / nTotalPx;
             nPixels_sat_smear = std::floor(CLHEP::RandBinomial::shoot(engine, nTotalPx, prob));
            if(nPixels_sat_smear > 0)
            {
               simHitADC = nPixels_sat_smear* gainValue;
            }
            else
            {simHitADC = 0.;}
         }
         else
         {
          simHitADC = nPixels * gainValue;
         }
      }
      else
      {
       simHitADC = nPixels * gainValue;
      }
        //store ADC
        chargeColl[i] = simHitADC;

        if (debug && cell.hit_info[0][i] > 0 )
        edm::LogVerbatim("HGCDigitizer") << "Realistic Sci Digitizer\n"
        				 << "keV " << cell.hit_info[0][i]
                                         << "\ntotalIniMIPs " << totalIniMIPs
                                         << "\nADC(9mm2 ConvG=2 OV=4) " << chargeColl[i]
                                         //<< "ADCthresh * ly = " << vanillaADCThr* scaledPePerMip
                                         << "\nvanillaADCThr = " << vanillaADCThr
                                         << "\n ADC thr " <<  thrADC
                                         << "\nnPixels " << nPixels
                                         << "\nnPixels_sat " << nPixels_sat
                                         << "\nnTotalPx " << nTotalPx
                                         << "\nnPixels_sat_smear " << nPixels_sat_smear
                                         << "\ngainValue " << gainValue
                                         << " cellId.granularity() " << scId.granularity()
                                        // << "\nradius "<< radius << " X: " << x << " Y: " << y << " Z: " << z
                                        // << "\nTileAreaFactor " << tileE << " 3/tileE = " << 3.0/ tileE << " Tile Area: " << tileA
                                        // << "\nHGCalHSc "<< (id.det() == DetId::HGCalHSc)
                                        // << "\nscaledPePerMip "<< scaledPePerMip 
                                        // << "\ngainIdx " << gainIdx
                                         << "\n\nid.rawId() " << id.rawId()
                                         << "\nSigma " << sigma
                                         << "\nscaledLYPerMip "<< scaledLYPerMip;


      //update time of arrival
      toa[i] = cell.hit_info[1][i];
      if (myFEelectronics_->toaMode() == HGCFEElectronics<HGCalDataFrame>::WEIGHTEDBYE && totalIniMIPs > 0)
        toa[i] = cell.hit_info[1][i] / totalIniMIPs;
    }

    //init a new data frame and run shaper
    HGCalDataFrame newDataFrame(id);
    //this->myFEelectronics_->runShaper(newDataFrame, chargeColl, toa, engine, thrADC, adcLsb, gainIdx, maxADC);  // for shaper with TOT
    this->myFEelectronics_->runSimpleShaper(newDataFrame, chargeColl, thrADC, adcLsb, gainIdx, maxADC); // UPD: simple shaper created and used for updated functions
    //prepare the output
    this->updateOutput(digiColl, newDataFrame);
  }
//outfile.close();
}
/// runRealisticSciDigitizer



//
void HGCHEbackDigitizer::runCaliceLikeDigitizer(std::unique_ptr<HGCalDigiCollection>& digiColl,
                                                HGCSimHitDataAccumulator& simData,
                                                const CaloSubdetectorGeometry* theGeom,
                                                const std::unordered_set<DetId>& validIds,
                                                CLHEP::HepRandomEngine* engine) {
  //switch to true if you want to print some details
  constexpr bool debug(false);

  HGCSimHitData chargeColl, toa;

  // this represents a cell with no signal charge
  HGCCellInfo zeroData;
  zeroData.hit_info[0].fill(0.f);  //accumulated energy
  zeroData.hit_info[1].fill(0.f);  //time-of-flight

  for (const auto& id : validIds) {
    chargeColl.fill(0.f);
    HGCSimHitDataAccumulator::iterator it = simData.find(id);
    HGCCellInfo& cell = (simData.end() == it ? zeroData : it->second);
    addCellMetadata(cell, theGeom, id);

    for (size_t i = 0; i < cell.hit_info[0].size(); ++i) {
      //convert total energy keV->MIP, since converted to keV in accumulator
      const float totalIniMIPs(cell.hit_info[0][i] * keV2MIP_);

      //generate random number of photon electrons
      const uint32_t npe = std::floor(CLHEP::RandPoissonQ::shoot(engine, totalIniMIPs * nPEperMIP_));

      //number of pixels
      const float x = vdt::fast_expf(-((float)npe) / nTotalPE_);
      uint32_t nPixel(0);
      if (xTalk_ * x != 1)
        nPixel = (uint32_t)std::max(nTotalPE_ * (1.f - x) / (1.f - xTalk_ * x), 0.f);

      //update signal
      if (sdPixels_ != 0)
        nPixel = (uint32_t)std::max(CLHEP::RandGaussQ::shoot(engine, (double)nPixel, sdPixels_), 0.);

      //convert to MIP again and saturate
      float totalMIPs(0.f), xtalk = 0.f;
      const float peDiff = nTotalPE_ - (float)nPixel;
      if (peDiff != 0.f) {
        xtalk = (nTotalPE_ - xTalk_ * ((float)nPixel)) / peDiff;
        if (xtalk > 0.f && nPEperMIP_ != 0.f)
          totalMIPs = (nTotalPE_ / nPEperMIP_) * vdt::fast_logf(xtalk);
      }

      //add noise (in MIPs)
      chargeColl[i] = totalMIPs;
      if (noise_MIP_ != 0)
        chargeColl[i] += std::max(CLHEP::RandGaussQ::shoot(engine, 0., noise_MIP_), 0.);
      if (debug && cell.hit_info[0][i] > 0)
        edm::LogVerbatim("HGCDigitizer") << "[runCaliceLikeDigitizer] xtalk=" << xtalk << " En=" << cell.hit_info[0][i]
                                         << " keV -> " << totalIniMIPs << " raw-MIPs -> " << chargeColl[i]
                                         << " digi-MIPs";
    }

    //init a new data frame and run shaper
    HGCalDataFrame newDataFrame(id);
    this->myFEelectronics_->runShaper(newDataFrame, chargeColl, toa, engine);

    //prepare the output
    this->updateOutput(digiColl, newDataFrame);
  }
}


//
void HGCHEbackDigitizer::runProperCaliceDigitizer(std::unique_ptr<HGCalDigiCollection>& digiColl,
                                                HGCSimHitDataAccumulator& simData,
                                                const CaloSubdetectorGeometry* theGeom,
                                                const std::unordered_set<DetId>& validIds,
                                                CLHEP::HepRandomEngine* engine) {
  
/*
Digitizer implementation following chain: 
dE SimHit -> #MIPs SimHit via energy value of MPV for a MIP -> #Pixels SimHit via Light Yield -> saturate & poisson smearing -> ADC of SimHit via gain
(-> check is ADC is too large for the ADC scale -> set ADC to max)

This should emulate what is happening in the Tile, SiPM and chip to rpoduce the ADC data (that we would get in real life). 
Thus, this should use fixed constants (fix parametrization) to the best of our knowledge. 

At first dealing only with the ADC scale. 

!!!!!!!!!!!! TODO: ToT regime should also be handled here, needs proper description. Original CALICE code in case of saturation of the ADC just set the ADC to max.
!!!!!!!!!!!! 	   Firts implementation of this chain for CMSSW HGCAL Scintillator digitization will use the CALICE approach for now. 

The list of required variables:

MeV2MIP = 0.455 				// [MeV] -  MPV value of the Energy deposition of the MIP in a tile (3mm plastic scintillator tile)
						// The value of 455 keV is taken from a standalone Geant4 simulation of MIP muons through a 3mm thick BC-412 tile. 
						// Incident muons perpendicular to the tile surface.
		
lightYield = Func(tile size, tile material) 	// [p.e.] - number of photoelections generated in the SiPM for a MIP incident upon SiPM-on-tile.
					    	// Parametrised from QC measurements for size and material dependence 

TotalNpix = 39984 				// Number of pixels of the SiPM used. The example number is for Hamamatsu S14160 SiPM series with 3x3 mm2 area and 15um pitch


gainValue = 7.55				// [ADC] - gain of the used SiPM in ADC units. Always for a specific overvoltage (OV) and ConveyorGain (CG). 
						// Defined as the distance between peaks of the single pixel spectrum (SPS) measured with the ROC.  
						// Example gain 7.55 ADC is for 4V OV, CG 12, 9mm2 SiPM 

maxADC = 1024					// [ADC] - the maximum value of the ADC. Example 1024 correspond to the 10bit ADC of the HGCROC3c. 

This chain will produce DIGIs in ADC.

The reverse of this chain should happen in reconstruction to produce Uncalibrated Rec Hits (with steps of applying cuts for ADC threshold and de-saturation). Producing number of reconstructed MIPs. Later at the RecHit stage, weights in units of energy are to be applied to correct for the absorber thickness (energy calibration on the shower scale) and to get reconstructed hits in energy.
*/  


  
  //switch to true if you want to print some details
  constexpr bool debug(false);
  constexpr bool doSaturation(true);    // flag to perform emulation of saturation of SiPM pixels
  constexpr bool doSmearing(true);      // flag to perform emulation of statistical fluctiations of n of fired pixels
  float vanillaADCThr = this->myFEelectronics_->getADCThreshold();  //vanilla thrs  in MIPs 
  float ADCThr = vanillaADCThr*lightYield_id;
  float adcLsb(this->myFEelectronics_->getADClsb());
  int gainIdx(0);

  HGCSimHitData chargeColl, toa;

  // this represents a cell with no signal charge
  HGCCellInfo zeroData;
  zeroData.hit_info[0].fill(0.f);  //accumulated energy
  zeroData.hit_info[1].fill(0.f);  //time-of-flight

  for (const auto& id : validIds) 
{
    chargeColl.fill(0.f);
    toa.fill(0.f);
    HGCSimHitDataAccumulator::iterator it = simData.find(id);
    HGCCellInfo& cell = (simData.end() == it ? zeroData : it->second);
    addCellMetadata(cell, theGeom, id);
    float simHitADC(0.f);  //simHitADC = 0.;

    for (size_t i = 0; i < cell.hit_info[0].size(); ++i) {
      //convert total energy keV->MIP, since converted to keV in accumulator
       float totalIniMIPs(cell.hit_info[0][i] * keV2MIP_);
      
      //lightYield_id = LYfunc(detID);  // need to get the LY value for the given tile by detID
      // will be constant for now
       float nPixels = totalIniMIPs * lightYield_id;
      
      if(doSaturation){
         
         //const uint32_t nPixels_sat = saturate(nPixels);
         
          float nPixels_sat = nTotalPx * (1 - vdt::fast_expf(-nPixels/ nTotalPx));
         
         if(doSmearing){
             float prob = nPixels_sat / nTotalPx;
             uint32_t nPixels_sat_smear = std::floor(CLHEP::RandBinomial::shoot(engine, nTotalPx, prob));
            if(nPixels_sat_smear > 0)
            {
               simHitADC = nPixels_sat_smear; //* gainValue;
            }
            else
            {simHitADC = 0.;}				
      
      
         }
         else
         {
          simHitADC = nPixels;// * gainValue;
         }
      }
      else 
      {
       simHitADC = nPixels;// * gainValue;
      }
      
      // if above ADC range
      // for now just put the max value
      /*
      if(simHitADC > maxADC)
      {
       simHitADC = maxADC;
      }
      */


      chargeColl[i] = simHitADC/lightYield_id;
      if (debug && cell.hit_info[0][i] > 0)
        edm::LogVerbatim("HGCDigitizer") << " En=" << cell.hit_info[0][i]
                                         << " keV -> " << totalIniMIPs << " raw-MIPs -> " << chargeColl[i]
                                         << " digi-MIPs"
                                         << " ADCthresh * ly = " << ADCThr* lightYield_id
                                         << " ADCthresh = " << ADCThr;
    }
    
        //init a new data frame and use no shaper: since the proper conversion to ADC is done here and no shape effects are interesting atm
        // need to make sure the no shaper is chosen!
    HGCalDataFrame newDataFrame(id);
    this->myFEelectronics_->runMinShaper(newDataFrame, chargeColl, engine, ADCThr, gainValue*lightYield_id, maxADC);
    //this->myFEelectronics_->runShaper(newDataFrame, chargeColl, toa, adcPulse, engine, ADCThr* lightYield_id, adcLsb, gainIdx, gainValue, maxADC);

    //prepare the output
    this->updateOutput(digiColl, newDataFrame);

}


}



//
HGCHEbackDigitizer::~HGCHEbackDigitizer() {}

DEFINE_EDM_PLUGIN(HGCDigitizerPluginFactory, HGCHEbackDigitizer, "HGCHEbackDigitizer");
