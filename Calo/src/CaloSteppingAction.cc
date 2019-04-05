// to make hits in EB/EE/HC
#include "SimGVCore/Calo/interface/CaloSteppingAction.h"
#include "SimG4Core/Notification/interface/G4TrackToParticleID.h"

#include "DataFormats/HcalDetId/interface/HcalDetId.h"
#include "SimDataFormats/CaloHit/interface/PCaloHit.h"

#include "FWCore/Framework/interface/EventSetup.h"
#include "FWCore/Framework/interface/ESHandle.h"
#include "FWCore/MessageLogger/interface/MessageLogger.h"
#include "FWCore/Utilities/interface/isFinite.h"
#include "FWCore/Utilities/interface/Exception.h"

#include "G4LogicalVolumeStore.hh"
#include "G4NavigationHistory.hh"
#include "G4ParticleTable.hh"
#include "G4PhysicalVolumeStore.hh"
#include "G4RegionStore.hh"
#include "G4Trap.hh"
#include "G4UnitsTable.hh"
#include "G4SystemOfUnits.hh"

#include <cmath>
#include <iostream>
#include <iomanip>

//#define EDM_ML_DEBUG

template <class Traits>
CaloSteppingAction<Traits>::CaloSteppingAction(const edm::ParameterSet &p) : 
  count_(0) {

  edm::ParameterSet iC = p.getParameter<edm::ParameterSet>("CaloSteppingAction");
  nameEBSD_       = iC.getParameter<std::vector<std::string> >("EBSDNames");
  nameEESD_       = iC.getParameter<std::vector<std::string> >("EESDNames");
  nameHCSD_       = iC.getParameter<std::vector<std::string> >("HCSDNames");
  nameHitC_       = iC.getParameter<std::vector<std::string> >("HitCollNames");
  slopeLY_        = iC.getParameter<double>("SlopeLightYield");
  birkC1EC_       = iC.getParameter<double>("BirkC1EC")*(g/(MeV*cm2));
  birkSlopeEC_    = iC.getParameter<double>("BirkSlopeEC");
  birkCutEC_      = iC.getParameter<double>("BirkCutEC");
  birkC1HC_       = iC.getParameter<double>("BirkC1HC")*(g/(MeV*cm2));
  birkC2HC_       = iC.getParameter<double>("BirkC2HC");
  birkC3HC_       = iC.getParameter<double>("BirkC3HC");

  edm::LogVerbatim("Step") << "CaloSteppingAction:: " << nameEBSD_.size() 
                           << " names for EB SD's";
  for (unsigned int k=0; k<nameEBSD_.size(); ++k)
    edm::LogVerbatim("Step") << "[" << k << "] " << nameEBSD_[k];
  edm::LogVerbatim("Step") << "CaloSteppingAction:: " << nameEESD_.size() 
                           << " names for EE SD's";
  for (unsigned int k=0; k<nameEESD_.size(); ++k)
    edm::LogVerbatim("Step") << "[" << k << "] " << nameEESD_[k];
  edm::LogVerbatim("Step") << "CaloSteppingAction:: " << nameHCSD_.size() 
                           << " names for HC SD's";
  for (unsigned int k=0; k<nameHCSD_.size(); ++k)
    edm::LogVerbatim("Step") << "[" << k << "] " << nameHCSD_[k];
  edm::LogVerbatim("Step") << "CaloSteppingAction::Constants for ECAL: slope "
                           << slopeLY_ << " Birk constants " << birkC1EC_ 
                           << ":" << birkSlopeEC_ << ":" << birkCutEC_;
  edm::LogVerbatim("Step") << "CaloSteppingAction::Constants for HCAL: Birk "
                           << "constants " << birkC1HC_ << ":" << birkC2HC_
                           << ":" << birkC3HC_;
  edm::LogVerbatim("Step") << "CaloSteppingAction:: " << nameHitC_.size() 
                           << " hit collections";
  for (unsigned int k=0; k<nameHitC_.size(); ++k)
    edm::LogVerbatim("Step") << "[" << k << "] " << nameHitC_[k];

  ebNumberingScheme_ = std::make_unique<EcalBarrelNumberingScheme>();
  eeNumberingScheme_ = std::make_unique<EcalEndcapNumberingScheme>();
  hcNumberingPS_     = std::make_unique<HcalNumberingFromPS>(iC);
  hcNumberingScheme_ = std::make_unique<HcalNumberingScheme>();
  for (int k=0; k<CaloSteppingAction::nSD_; ++k) {
    slave_[k] = std::make_unique<CaloSlaveSD>(nameHitC_[k]);
    produces<edm::PCaloHitContainer>(nameHitC_[k]);
  }
} 

template <class Traits>
CaloSteppingAction<Traits>::~CaloSteppingAction() {
  edm::LogVerbatim("Step") << "CaloSteppingAction: -------->  Total number of "
                           << "selected entries : " << count_;
}

template <class Traits>
void CaloSteppingAction<Traits>::produce(edm::Event& e, const edm::EventSetup&) {

  for (int k=0; k<CaloSteppingAction::nSD_; ++k) {
    saveHits(k);
    auto product = std::make_unique<edm::PCaloHitContainer>();
    fillHits(*product,k);
    e.put(std::move(product),nameHitC_[k]);
  }
}

template <class Traits>
void CaloSteppingAction<Traits>::fillHits(edm::PCaloHitContainer& cc, int type) {
  edm::LogVerbatim("Step") << "CaloSteppingAction::fillHits for type "
                           << type << " with "
                           << slave_[type].get()->hits().size() << " hits";
  cc = slave_[type].get()->hits();
  slave_[type].get()->Clean();
}

template <class Traits>
void CaloSteppingAction<Traits>::update(const BeginOfJob * job) {
  edm::LogVerbatim("Step") << "CaloSteppingAction:: Enter BeginOfJob";
}

//==================================================================== per RUN
template <class Traits>
void CaloSteppingAction<Traits>::update(const BeginRunWrapper& run) {

  int irun = run.getRunID();
  edm::LogVerbatim("Step") << "CaloSteppingAction:: Begin of Run = " << irun;

  const auto& nameMap = VolumeWrapper::getVolumes();
    for (auto const& name : nameEBSD_) {
      for (const auto& itr : nameMap) {
        const std::string &lvname = itr.first;
        if (lvname.find(name) != std::string::npos) {
          volEBSD_.emplace_back(itr.second);
          int type =  (lvname.find("refl") == std::string::npos) ? -1 : 1;
          double dz = VolumeWrapper(itr.second).dz();
          xtalMap_.emplace(itr.second,dz*type);
        }
      }
    }
    for (auto const& name : nameEESD_) {
      for (const auto& itr : nameMap) {
        const std::string &lvname = itr.first;
        if (lvname.find(name) != std::string::npos)  {
          volEESD_.emplace_back(itr.second);
          int type =  (lvname.find("refl") == std::string::npos) ? 1 : -1;
          double dz = VolumeWrapper(itr.second).dz();
          xtalMap_.emplace(itr.second,dz*type);
        }
      }
    }
    for (auto const& name : nameHCSD_) {
      for (const auto& itr : nameMap) {
        const std::string &lvname = itr.first;
        if (lvname.find(name) != std::string::npos) 
          volHCSD_.emplace_back(itr.second);
      }
    }
#ifdef EDM_ML_DEBUG
  edm::LogVerbatim("Step") << volEBSD_.size() << " logical volumes for EB SD";
  for (unsigned int k=0; k<volEBSD_.size(); ++k)
    edm::LogVerbatim("Step") << "[" << k << "] " << volEBSD_[k];
  edm::LogVerbatim("Step") << volEESD_.size() << " logical volumes for EE SD";
  for (unsigned int k=0; k<volEESD_.size(); ++k)
    edm::LogVerbatim("Step") << "[" << k << "] " << volEESD_[k];
  edm::LogVerbatim("Step") << volHCSD_.size() << " logical volumes for HC SD";
  for (unsigned int k=0; k<volHCSD_.size(); ++k)
    edm::LogVerbatim("Step") << "[" << k << "] " << volHCSD_[k];
#endif
}

//=================================================================== per EVENT
template <class Traits>
void CaloSteppingAction<Traits>::update(const BeginEventWrapper& evt) {
 
  eventID_ = evt.getEventID();
  edm::LogVerbatim("Step") <<"CaloSteppingAction: Begin of event = " 
                           << eventID_;
  for (int k=0; k<CaloSteppingAction<Traits>::nSD_; ++k) {
    hitMap_[k].erase (hitMap_[k].begin(), hitMap_[k].end());
    slave_[k].get()->Initialize();
  }
}

//=================================================================== each STEP
template <class Traits>
void CaloSteppingAction<Traits>::update(const StepWrapper& aStep) {

  //  edm::LogVerbatim("Step") <<"CaloSteppingAction: At each Step";
  NaNTrap(aStep);
  auto lv = aStep.getVolume();
  bool hc = (std::find(volHCSD_.begin(),volHCSD_.end(),lv)!=volHCSD_.end());
  bool eb = (std::find(volEBSD_.begin(),volEBSD_.end(),lv)!=volEBSD_.end());
  bool ee = (std::find(volEESD_.begin(),volEESD_.end(),lv)!=volEESD_.end());
  if  (hc || eb || ee) {
    double dEStep = aStep.getEnergyDeposit();
    double     time     = aStep.getTime()/nanosecond;
    int        primID   = aStep.getTrackID();
    bool       em       = aStep.getEM();
    if (hc) {
      int depth = (aStep.getCopyNo(0))%10 + 1;
      int lay   = (aStep.getCopyNo(0)/10)%100 + 1;
      int det   = (aStep.getCopyNo(1))/1000;
      auto unitID = getDetIDHC(det, lay, depth, aStep.getPosition(false));
      if(unitID > 0 && dEStep > 0.0) {
        dEStep *= getBirkHC(dEStep, aStep.getStepLength(), aStep.getCharge(), aStep.getDensity());
        fillHit(unitID, dEStep, time, primID, 0, em, 2);
      }
    } else {
      EcalBaseNumber theBaseNumber;
      int  size = aStep.getSize();
      if (theBaseNumber.getCapacity() < size ) theBaseNumber.setSize(size);
      //Get name and copy numbers
      if (size > 1) {
        for (int ii = 0; ii < size ; ii++) {
          const auto& nameNumber = aStep.getNameNumber(ii);
          theBaseNumber.addLevel(nameNumber.first, nameNumber.second);
        }
      }
      auto unitID = (eb ? (ebNumberingScheme_->getUnitID(theBaseNumber)) :
                     (eeNumberingScheme_->getUnitID(theBaseNumber)));
      if (unitID > 0 && dEStep > 0.0) {
        double dz = aStep.getDz();
        auto ite   = xtalMap_.find(lv);
        double crystalLength = ((ite == xtalMap_.end()) ? 230.0 : 
                                std::abs(ite->second));
        double crystalDepth = ((ite == xtalMap_.end()) ? 0.0 :
                               (std::abs(0.5*(ite->second)+dz)));
        double radl   = aStep.getRadlen();
        bool   flag   = ((ite == xtalMap_.end()) ? true : (((ite->second) >= 0)
                                                           ? true : false));
        auto   depth  = getDepth(flag, crystalDepth, radl);
        dEStep        *= (getBirkL3(dEStep,aStep.getStepLength(), aStep.getCharge(), aStep.getDensity()) * 
                          curve_LY(crystalLength,crystalDepth));
        fillHit(unitID, dEStep, time, primID, depth, em, (eb ? 0 : 1));
      }
    }
  }
}

//================================================================ End of EVENT
template <class Traits>
void CaloSteppingAction<Traits>::update(const EndEventWrapper& evt) {
  ++count_;
  // Fill event input 
  edm::LogVerbatim("Step") << "CaloSteppingAction: EndOfEvent " 
                           << evt.getEventID();
}

template <class Traits>
void CaloSteppingAction<Traits>::NaNTrap(const StepWrapper& aStep) const {

  auto currentPos = aStep.getPosition(true);
  double xyz = currentPos.x() + currentPos.y() + currentPos.z();
  auto currentMom = aStep.getMomentum();
  xyz += currentMom.x() + currentMom.y() + currentMom.z();

  if (edm::isNotFinite(xyz)) {
    const auto& nameOfVol = aStep.getVolumeName();
    throw cms::Exception("Unknown", "CaloSteppingAction") 
      << " Corrupted Event - NaN detected in volume " << nameOfVol << "\n";
  }
}

template <class Traits>
uint32_t CaloSteppingAction<Traits>::getDetIDHC(int det, int lay, int depth,
                                        const math::XYZVectorD& pos) const {

  HcalNumberingFromDDD::HcalID tmp = hcNumberingPS_.get()->unitID(det, lay, 
                                                                  depth, pos);
  return (hcNumberingScheme_.get()->getUnitID(tmp));
}

template <class Traits>
void CaloSteppingAction<Traits>::fillHit(uint32_t id, double dE, double time,
                                 int primID, uint16_t depth, double em,
                                 int flag) {
  CaloHitID  currentID(id, time, primID, depth);
  double edepEM  = (em ? dE : 0);
  double edepHAD = (em ? 0 : dE);
  std::pair<int,CaloHitID> evID = std::make_pair(eventID_,currentID);
  auto it = hitMap_[flag].find(evID);
  if (it != hitMap_[flag].end()) {
    (it->second).addEnergyDeposit(edepEM,edepHAD);
  } else {
    CaloGVHit aHit;
    aHit.setEventID(eventID_);
    aHit.setID(currentID);
    aHit.addEnergyDeposit(edepEM,edepHAD);
    hitMap_[flag][evID] = aHit;
  }
}

template <class Traits>
uint16_t CaloSteppingAction<Traits>::getDepth(bool flag, double crystalDepth,
                                      double radl) const {
  uint16_t depth1 = (flag ? 0 : PCaloHit::kEcalDepthRefz);
  uint16_t depth2 = (uint16_t)floor(crystalDepth/radl);
  uint16_t depth  = (((depth2&PCaloHit::kEcalDepthMask) << PCaloHit::kEcalDepthOffset) | depth1);
#ifdef EDM_ML_DEBUG
  edm::LogVerbatim("Step") << "CaloSteppingAction::getDepth radl " << radl
                           << ":" << crystalDepth << " depth " << depth;
#endif
  return depth;
}

template <class Traits>
double CaloSteppingAction<Traits>::curve_LY(double crystalLength, 
                                    double crystalDepth) const {
  double weight = 1.;
  double dapd = crystalLength - crystalDepth;
  if (dapd >= -0.1 || dapd <= crystalLength+0.1) {
    if (dapd <= 100.)
      weight = 1.0 + slopeLY_ - dapd * 0.01 * slopeLY_;
#ifdef EDM_ML_DEBUG
    edm::LogVerbatim("Step") << "CaloSteppingAction::curve_LY " << crystalDepth
                             << ":" << crystalLength << ":" << dapd << ":" 
                             << weight;
#endif
  } else {
    edm::LogWarning("Step") << "CaloSteppingAction: light coll curve : wrong "
                            << "distance to APD " << dapd << " crlength = " 
                            << crystalLength <<" crystal Depth = " 
                            << crystalDepth << " weight = " << weight;
  }
  return weight;
}

template <class Traits>
double CaloSteppingAction<Traits>::getBirkL3(double dEStep, double step, 
                                     double charge, double density) const {
  double weight = 1.;
  if (charge != 0. && step > 0.) {
    double dedx    = dEStep/step;
    double rkb     = birkC1EC_/density;
    if (dedx > 0) {
      weight         = 1. - birkSlopeEC_*log(rkb*dedx);
      if (weight < birkCutEC_) weight = birkCutEC_;
      else if (weight > 1.)    weight = 1.;
    }
#ifdef EDM_ML_DEBUG
    edm::LogVerbatim("Step") << "CaloSteppingAction::getBirkL3 Charge "
                             << charge << " dE/dx " << dedx
                             << " Birk Const " << rkb << " Weight = " << weight
                             << " dE " << dEStep << " step " << step;
#endif
  }
  return weight;
}

template <class Traits>
double CaloSteppingAction<Traits>::getBirkHC(double dEStep, double step, double charge,
                                     double density) const {

  double weight = 1.;
  if (charge != 0. && step > 0.) {
    double dedx    = dEStep/step;
    double rkb     = birkC1HC_/density;
    double c       = birkC2HC_*rkb*rkb;
    if (std::abs(charge) >= 2.) rkb /= birkC3HC_;
    weight = 1./(1.+rkb*dedx+c*dedx*dedx);
#ifdef EDM_ML_DEBUG
    edm::LogVerbatim("Step") << "CaloSteppingAction::getBirkHC Charge " 
                             << charge << " dE/dx " << dedx 
                             << " Birk Const " << rkb << ", " << c 
                             << " Weight = " << weight << " dE "
                             << dEStep;
#endif
  }
  return weight;
}

template <class Traits>
void CaloSteppingAction<Traits>::saveHits(int type) {

  edm::LogVerbatim("Step") << "CaloSteppingAction:: saveHits for type " 
                           << type << " with " << hitMap_[type].size()
                           << " hits";
  slave_[type].get()->ReserveMemory(hitMap_[type].size());
  for (auto const& hit : hitMap_[type]) {
    slave_[type].get()->processHits(hit.second.getUnitID(),
                                    hit.second.getEM()/GeV, 
                                    hit.second.getHadr()/GeV,
                                    hit.second.getTimeSlice(),
                                    hit.second.getTrackID(),
                                    hit.second.getDepth());
  }
}

#include "SimGVCore/Calo/interface/G4Traits.h"
#include "SimGVCore/Calo/interface/GVTraits.h"

template class CaloSteppingAction<G4Traits>;
template class CaloSteppingAction<GVTraits>;
