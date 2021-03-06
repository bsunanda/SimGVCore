import FWCore.ParameterSet.Config as cms
from FWCore.ParameterSet.VarParsing import VarParsing

options = VarParsing("analysis")
options.register("particle", "electron", VarParsing.multiplicity.singleton, VarParsing.varType.string)
options.register("mult", 1, VarParsing.multiplicity.singleton, VarParsing.varType.int) # number of particles
options.register("pt", 1, VarParsing.multiplicity.singleton, VarParsing.varType.float)
options.register("sim", "Geant4", VarParsing.multiplicity.singleton, VarParsing.varType.string)
options.register("threads", 1, VarParsing.multiplicity.singleton, VarParsing.varType.int)
options.register("streams", 0, VarParsing.multiplicity.singleton, VarParsing.varType.int)
options.register("year", 2018, VarParsing.multiplicity.singleton, VarParsing.varType.int)
options.register("maxEventsIn", -1, VarParsing.multiplicity.singleton, VarParsing.varType.int)
options.parseArguments()

# choose particle
options._pdgid = 0
if options.particle=="electron": options._pdgid = 11
elif options.particle=="photon": options._pdgid = 22
else: raise ValueError("Unsupported particle: "+options.particle)

# check options
if options.streams == 0: options.streams = options.threads
if options.sim!="Geant4" and options.sim!="GeantV":
    raise ValueError("Unsupported sim: "+options.sim)
if options.year!=2018 and options.year!=2023:
    raise ValueError("Unsupported year: "+str(options.year))
if options.maxEventsIn==-1: options.maxEventsIn = options.maxEvents

# basic name definition
nametmp = options.particle+"_pt"+str(options.pt)+"_mult"+str(options.mult)
# gen name definition
options._genname = "gen_"+nametmp+"_n"+str(options.maxEventsIn)
# sim name definition
options._simname = "sim_"+options.sim+"_year"+str(options.year)+"_"+nametmp+"_n"+str(options.maxEvents)+"_th"+str(options.threads)+"_st"+str(options.streams)
