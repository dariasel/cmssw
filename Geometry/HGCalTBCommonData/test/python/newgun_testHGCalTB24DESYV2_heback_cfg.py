saveOnlyClusters=False

import FWCore.ParameterSet.Config as cms
from Configuration.Eras.Era_Phase2C26I13M9_cff import Phase2C26I13M9
process = cms.Process('GENSIMDIGIRECO',Phase2C26I13M9)


from FWCore.ParameterSet.VarParsing import VarParsing
options = VarParsing ('analysis')


options.register ('seed',
				  1,
				  VarParsing.multiplicity.singleton,
				  VarParsing.varType.float,
				  "Random seed")
options.register ('board',
				  "E", # module type, e.g. E-module, Rings R26-R32
				  VarParsing.multiplicity.singleton,
				  VarParsing.varType.string,
				  "Board type")
options.register ('sigma',
				  5.0, # %
				  VarParsing.multiplicity.singleton,
				  VarParsing.varType.float,
				  "rounded sigma of LY")
options.register ('ev',
				  1, # Number of events
				  VarParsing.multiplicity.singleton,
				  VarParsing.varType.int,
				  "Number of events")
options.register ('energy',
				  10.0, # 10 GeV
				  VarParsing.multiplicity.singleton,
				  VarParsing.varType.float,
				  "Particle Energy")
options.register ('layer',
				  34, # layer 34 is the first hgcal scintillator layer (with HD tiles)
				  VarParsing.multiplicity.singleton,
				  VarParsing.varType.int,
				  "Z position")
options.register ('particle',
				  13, # muon if nothing is provided
				  VarParsing.multiplicity.singleton,
				  VarParsing.varType.int,
				  "Particle id. Available options 13 (muon), 211 (pi+), 111 (pi0), 11 (e-)")
options.parseArguments()

layer_z={34: 406.0, 35:412.0, 36: 419.0, 37: 425.0,38:431.0, 39:440.0, 40:448.0, 41:456.0, 42:464.0, 43:472.0, 44:481.0, 45:489.0, 46:497.0, 47:505.0} 
# layer int used in the name, z in the gun
board_r={"A": 110.0,"B":135.0, "D":170.0, "E":200.0, "G":240.0}
board_rminmax={"A": [103.0, 117.0],"B":[117.0, 152.0], "D":[152.0, 181.0], "E":[181.0, 215.0], "G":[215.0, 259.0]} # mapped to use the string in the filename and radii in the gun
part_name={211 : "pion-plus", 111 : "pion0", 13 : "muon", 11 : "electron"} # mapped to use the PID in the gun and the string in the filename
#particle=13 #211 #111 #pi0   211 #pi+   13 #muon    11 #electron

particleEnergy=options.energy
ev=options.ev
layer=int(options.layer)
board=str(options.board)
rad = board_r[board]
radmin = board_rminmax[board][0]
radmax = board_rminmax[board][1]
zpos = layer_z[layer]

particle=int(options.particle)

particle_name=str(part_name[particle])


# import of standard configurations

process.load("FWCore.MessageService.MessageLogger_cfi")
process.load('Configuration.StandardSequences.Services_cff')
process.load('SimGeneral.HepPDTESSource.pythiapdt_cfi')
process.load('Configuration.EventContent.EventContent_cff')
process.load('Geometry.HGCalTBCommonData.testTB24DESYV2_heback_XML_cfi')
process.load('Geometry.HGCalCommonData.hgcalNumberingInitialization_cfi')
process.load('Geometry.HGCalCommonData.hgcalParametersInitialization_cfi')
process.load('Geometry.CaloEventSetup.HGCalTopology_cfi')
process.load('Geometry.CaloEventSetup.CaloTopology_cfi')
process.load('Geometry.CaloEventSetup.CaloGeometryBuilder_cfi')
process.CaloGeometryBuilder = cms.ESProducer( "CaloGeometryBuilder",
   SelectedCalos = cms.vstring("HGCalEESensitive", "HGCalHESiliconSensitive", "HGCalHEScintillatorSensitive")#, "HGCalHFNoseSensitive")
)
process.load('Geometry.HGCalGeometry.HGCalGeometryESProducer_cfi')
process.load('Configuration.StandardSequences.MagneticField_0T_cff')
process.load('Configuration.StandardSequences.Generator_cff')
process.load('GeneratorInterface.Core.generatorSmeared_cfi')
process.load('IOMC.EventVertexGenerators.VtxSmearedFlat_cfi')
process.load('GeneratorInterface.Core.genFilterSummary_cff')
process.load('Configuration.StandardSequences.SimIdeal_cff')
process.load('SimG4CMS.HGCalTestBeam.DigiHGCalTB24DESYV2_cff')
process.load('RecoLocalCalo.Configuration.hgcalLocalReco_cff')
process.load('Configuration.StandardSequences.Validation_cff')
process.load('Configuration.StandardSequences.EndOfProcess_cff')
process.load('Configuration.StandardSequences.FrontierConditions_GlobalTag_cff')

process.maxEvents = cms.untracked.PSet(
    input = cms.untracked.int32(ev)    #Number of Events; set as an argument
)

if 'MessageLogger' in process.__dict__:
    process.MessageLogger.G4cerr=dict()
    process.MessageLogger.G4cout=dict()
    #process.MessageLogger.HGCSim=dict()
    #process.MessageLogger.CaloSim=dict()
    #process.MessageLogger.FlatThetaGun=dict()
    #process.MessageLogger.FlatEvtVtx=dict()

#process.MessageLogger.cout.enable = True
# Input source
process.source = cms.Source("EmptySource")

process.options = cms.untracked.PSet(
)

# Production Info
process.configurationMetadata = cms.untracked.PSet(
    annotation = cms.untracked.string('SingleElectronE100_cfi nevts:10'),
    name = cms.untracked.string('Applications'),
    version = cms.untracked.string('$Revision: 1.19 $')
)

# Output definition

process.EDMoutput = cms.OutputModule("PoolOutputModule",
    SelectEvents = cms.untracked.PSet(
        SelectEvents = cms.vstring('generation_step')
    ),
    dataset = cms.untracked.PSet(
        dataTier = cms.untracked.string('GEN-SIM-DIGI-RECO'),
        filterName = cms.untracked.string('')
    ),
    eventAutoFlushCompressedSize = cms.untracked.int32(5242880),
    fileName = cms.untracked.string('file:gensimdigireco_'+particle_name+'_'+str(int(particleEnergy))+'_board_'+board+'_z_'+str(layer)+'_seed_'+str(options.seed)+'_sigma_'+str(int(options.sigma))+'.root'),
##############################################################################################################
    #outputCommands = process.EDMEventContent.outputCommands,
    #outputCommands = process.FEVTDEBUGHLTEventContent.outputCommands,
    outputCommands = cms.untracked.vstring("keep *"),
    splitLevel = cms.untracked.int32(0)
)
if saveOnlyClusters:
  process.EDMoutput.outputCommands = cms.untracked.vstring("drop *","keep recoCaloClusters_*_*_*")

# Additional output definition for TBAnalyser
#process.TFileService = cms.Service("TFileService",
#                                   fileName = cms.string('TBGenSim.root')
#                                   )

process.NANOAODoutput = cms.OutputModule("NanoAODOutputModule",
    compressionAlgorithm = cms.untracked.string('LZMA'),
    compressionLevel = cms.untracked.int32(9),
    dataset = cms.untracked.PSet(
        dataTier = cms.untracked.string('NANOAOD'),
        filterName = cms.untracked.string('')
    ),
    fileName = cms.untracked.string('file:nanoaod.root'),
    outputCommands = process.NANOAODEventContent.outputCommands
)
process.NANOAODoutput.compressionAlgorithm = 'ZSTD'
process.NANOAODoutput.compressionLevel = 5

process.DQMoutput = cms.OutputModule("DQMRootOutputModule",
    dataset = cms.untracked.PSet(
        dataTier = cms.untracked.string('DQMIO'),
        filterName = cms.untracked.string('')
    ),
    fileName = cms.untracked.string('file:dqm.root'),
    outputCommands = process.DQMEventContent.outputCommands,
    splitLevel = cms.untracked.int32(0)
)

# Other statements
process.genstepfilter.triggerConditions=cms.vstring("generation_step")
from Configuration.AlCa.GlobalTag import GlobalTag
process.GlobalTag = GlobalTag(process.GlobalTag, 'auto:phase2_realistic_T35', '')

# vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
# changed the gun to shoot in a range of rings, corresponding to a specific module type
# Type is given as a letter argument and the mapped radii are used as inputs

process.generator = cms.EDProducer("FlatRandomAngleEGunProducer",
    AddAntiParticle = cms.bool(False),
    PGunParameters = cms.PSet(
        MinE = cms.double(particleEnergy),  # GeV
        MaxE = cms.double(particleEnergy),
        
        MinTheta = cms.double(0.0),  #  along beam axis
        MaxTheta = cms.double(0.0), #1.57),  # orthogonal to beam axis

        MinPhi = cms.double(0.0),
        MaxPhi = cms.double(0.0),

        MinX = cms.double(0.0), # is used as angle alpha to calculate the ring of tiles/tilemodules
        MaxX = cms.double(6.282), # 0-6.282 is a full ring

        MinY = cms.double(radmin), #parameter-Module
        MaxY = cms.double(radmax),

	ZPosition = cms.double(zpos), # determined by the layer set as an argument
        PartID = cms.vint32(particle),  # Particle ID
       # PartID = cms.vint32((13 if particle == "muon" else 11)),  # Particle ID

        MinEta = cms.double(0.0), # DUMMY VALUE NOT USED
        MaxEta = cms.double(0.0), # DUMMY VALUE NOT USED
    ),
    Verbosity = cms.untracked.int32(1),
    firstRun = cms.untracked.uint32(1),
    psethack = cms.string('single electron E 100')
)


#process.generator = cms.EDProducer("FlatRandomEThetaGunProducer",
#    AddAntiParticle = cms.bool(False),
#    PGunParameters = cms.PSet(
#        MinE = cms.double(99.99),
#        MaxE = cms.double(100.01),
#        MinTheta = cms.double(0.0),
#        MaxTheta = cms.double(0.0),
#        MinPhi = cms.double(-3.14159265359),
#        MaxPhi = cms.double(3.14159265359),
#        PartID = cms.vint32(11) # 11 for electrons, 2212 for protons
#    ),
#    Verbosity = cms.untracked.int32(1),
#    firstRun = cms.untracked.uint32(1),
#    psethack = cms.string('single electron E 10')
#)

#process.generator = cms.EDProducer("BeamMomentumGunProducer",
#    AddAntiParticle = cms.bool(False),
#    PGunParameters = cms.PSet(
#        FileName = cms.FileInPath('SimG4CMS/HGCalTestBeam/data/HGCTBeamProfTree_PosE100.root'),
#        MinTheta = cms.double(0),
#        MaxTheta = cms.double(0),
#        MinPhi = cms.double(0),
#        MaxPhi = cms.double(0),
#        XOffset = cms.double(1.0), # 1cm away from middle
#        YOffset = cms.double(150.0), # somewwhere in the middle of HGCAL
#        ZPosition = cms.double(0.0),
#        PartID = cms.vint32(11)
#    ),
#    Verbosity = cms.untracked.int32(1),
#    firstRun = cms.untracked.uint32(1),
#    psethack = cms.string('single electron E 100')
#)
process.VtxSmeared.MinZ =  0.0
process.VtxSmeared.MaxZ =  0.0
process.VtxSmeared.MinX =  0.0
process.VtxSmeared.MaxX =  0.0
process.VtxSmeared.MinY =  0.0
process.VtxSmeared.MaxY =  0.0
process.VtxSmeared.MinT =  0.0
process.VtxSmeared.MaxT =  0.0
process.g4SimHits.OnlySDs = ['HGCalSensitiveDetector','HGCScintillatorSensitiveDetector', 'HGCSensitiveDetector'] #, 'HcalTB06BeamDetector','HFNoseSensitiveDetector']
process.g4SimHits.HGCSD.Detectors = 1
process.g4SimHits.HGCSD.RejectMouseBite = False
process.g4SimHits.HGCSD.RotatedWafer    = False

process.g4SimHits.CaloTrkProcessing.TestBeam = True
process.g4SimHits.HCalSD.ForTBHCAL = True
process.g4SimHits.NonBeamEvent = True
process.g4SimHits.UseMagneticField = False

process.g4SimHits.EventVerbose = 2
process.g4SimHits.SteppingVerbosity = 2
process.g4SimHits.StepVerboseThreshold= 0.1
process.g4SimHits.VerboseEvents = [1]
process.g4SimHits.VertexNumber = []
process.g4SimHits.VerboseTracks =[]

# Skip the alias simHGCalUnsupressedDigis and the hgcalDigis from HGCalRawToDigiFake producer
process.HGCalUncalibRecHit.HGCEEdigiCollection = cms.InputTag("mix","HGCDigisEE")
process.HGCalUncalibRecHit.HGCHEBdigiCollection = cms.InputTag("mix","HGCDigisHEback")
process.HGCalUncalibRecHit.HGCHEFdigiCollection = cms.InputTag("mix","HGCDigisHEfront")

#from PhysicsTools.NanoAOD.common_cff import Var
#process.hgcDigiHEbackTable = cms.EDProducer("SimpleHGCDigiFlatTableProducer",
#     src = cms.InputTag("mix","HGCDigisHEback"),
#     cut = cms.string(""), 
#     name = cms.string("HGCDigisHEback"),
#     doc  = cms.string("HGCAL hadronic scintillator digis"),
#     singleton = cms.bool(False), # the number of entries is variable
#     extension = cms.bool(False),
#     variables = cms.PSet(
#         rawId = Var('id().rawId()', 'uint', precision=-1, doc='raw id'),
#         raw = Var('sample(2).raw()', 'uint', doc='raw'),
#         threshold = Var('sample(2).threshold()', 'bool', doc='threshold'),
#         mode = Var('sample(2).mode()', 'bool', doc='mode'),
#         gain = Var('sample(2).gain()', 'uint16', doc='gain'),
#         toa = Var('sample(2).toa()', 'uint16', doc='toa'),
#         data = Var('sample(2).data()', 'uint16', doc='data'),
#         getToAValid = Var('sample(2).getToAValid()', 'bool', doc='getToAValid'),
#     )
#)
#process.hgcDigiHEfrontTable=process.hgcDigiHEbackTable.clone(src = cms.InputTag("mix","HGCDigisHEfront"))

# Path and EndPath definitions
process.generation_step = cms.Path(process.pgen)
process.simulation_step = cms.Path(process.psim)
process.genfiltersummary_step = cms.EndPath(process.genFilterSummary)
process.digitisation_step = cms.Path(process.mix)
process.reconstruction_step = cms.Path(cms.Sequence(process.HGCalUncalibRecHit*process.HGCalRecHit*process.recHitMapProducer*process.hgcalLayerClustersEE*process.hgcalLayerClustersHSi*process.hgcalLayerClustersHSci*process.hgcalMergeLayerClusters*process.hgcalMultiClusters))
process.reconstruction_short_step = cms.Path(cms.Sequence(process.HGCalUncalibRecHit*process.HGCalRecHit*process.hgcalLayerClustersEE*process.hgcalLayerClustersHSi*process.hgcalLayerClustersHSci*process.hgcalMergeLayerClusters))
#process.hgcalnano_step = cms.Path(cms.Sequence(process.hgcDigiHEbackTable*process.hgcHEbackRecHitsTable))#*cms.Sequence(process.hgcRecHitsTask))#*cms.Sequence(process.hgcCMDigiTable*process.unpackerFlagsTable))#*cms.Sequence(process.hgcSoaDigiTable))
#process.analysis_step = cms.Path(process.HGCalTB23Analyzer)
#process.prevalidation_step7 = cms.Path(process.globalPrevalidationHGCal)
#process.validation_step9 = cms.EndPath(process.hgcalSimHitValidationHEB+process.hgcalDigiValidationHEB+process.hgcalRecHitValidationHEB)#process.globalValidationHGCal)
process.endjob_step = cms.EndPath(process.endOfProcess)
process.EDMoutput_step = cms.EndPath(process.EDMoutput)
process.NANOAODoutput_step = cms.EndPath(process.NANOAODoutput)
process.DQMoutput_step = cms.EndPath(process.DQMoutput)

# Schedule definition
process.schedule = cms.Schedule(process.generation_step,
				process.genfiltersummary_step,
				process.simulation_step,
				process.digitisation_step,
				#process.reconstruction_short_step,
				process.reconstruction_step,
				#process.hgcalnano_step,
			        #process.analysis_step,
                                #process.prevalidation_step7,
                                #process.validation_step9,
				process.endjob_step,
				process.EDMoutput_step,
                                #process.NANOAODoutput_step,
                                #process.DQMoutput_step
				)
# filter all path with the production filter sequence
for path in process.paths:
    if "generation_step" in path: # Replace with your specific generation path name
        getattr(process, path)._seq = process.generator * getattr(process, path)._seq

from PhysicsTools.PatAlgos.tools.helpers import associatePatAlgosToolsTask
associatePatAlgosToolsTask(process)

# Automatic addition of the customisation function from DPGAnalysis.HGCalTools.tb2023_cfi
#from DPGAnalysis.HGCalTools.tb2023_cfi import addPerformanceReports,configTBConditions_default 

#call to customisation function addPerformanceReports imported from DPGAnalysis.HGCalTools.tb2023_cfi
#process = addPerformanceReports(process)

#call to customisation function configTBConditions_default imported from DPGAnalysis.HGCalTools.tb2023_cfi
#process = configTBConditions_default(process)

# attempt to calculate the dEdx correction for electromagnetic stack with steel absorbers
#radiation length: FromChrisdEdx['StainlessSteel'] = 1.14 in MeV/mm from https://github.com/cms-sw/cmssw/blob/master/SimTracker/TrackerMaterialAnalysis/test/dEdxWeights.ipynb
# time layer thickness 16mm
# --> dE=18.24 MeV
#print(len(process.hgcalLayerClustersHSci.plugin.dEdXweights))
#process.hgcalLayerClustersHSci.plugin.dEdXweights = cms.vdouble([1e-10 for i in range(51-15)]+[18.24 for i in range(15)]) # last 14 layers in HB
#print(len(process.hgcalLayerClustersHSci.plugin.dEdXweights))
#process.HGCalRecHit.layerWeights = process.hgcalLayerClustersHSci.plugin.dEdXweights

process.mix.digitizers.hgcalHEback.tofDelay=0 # line to adjust the digitizer to adjust for the time of arrival of particles shot from close to the detector
process.mix.digitizers.hgcalHEback.digiCfg.feCfg.adcThreshold_fC=0.25 #lowering the MIP threshold in digitizer

sigma_float = float(options.sigma)/100   #sigma is set in per cent e.g. as the argument, recalculated to be a fraction for the gaussian light yield variation
process.mix.digitizers.hgcalHEback.digiCfg.sdPixels=sigma_float # sdPixels parameter name used instead of creating a new called sigma for simplicity TODO: fix

process.RandomNumberGeneratorService.generator.initialSeed=int(options.seed)
process.RandomNumberGeneratorService.g4SimHits.initialSeed=int(options.seed)
print("Using random seed", int(options.seed))
from SimCalorimetry.HGCalSimProducers.hgcalDigitizer_cfi import *

#HGCal_setRealisticNoiseSci(process)
#HGCal_setEndOfLifeNoise(process)
#process.hgcalLayerClustersHSci.plugin.kappa=6 # disable clustering, by treating every hit as seed
#process.hgcalLayerClustersHSci.plugin.deltac=cms.vdouble(0.00001,0.00001,0.00001,0.00001) # disable clustering, by treating every hit as outlier
