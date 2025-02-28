include(CTest)

#
# add a custom SCIP check target 'scip_check'
#
add_custom_target(scip_check
                COMMAND ${CMAKE_CTEST_COMMAND} -R "-default" -E "applications" --output-on-failure
                DEPENDS scip
                )

#
# add a phony target check, if it has not been defined in a different SCIP Optimization Suite project, yet
# and add 'scip_check' as a dependency.
#
if (NOT TARGET check)
    add_custom_target(check)
endif()

add_dependencies(check scip_check)

# copy data files to binary directory, so that they can be accessed with relative paths
# in settings files
file(COPY coverage/data DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/coverage)
#
# define the instance sets
#
# semicolon '\;' is used to split an instance and its optimal objective value
# For infeasible instances, '+infinity' is used (or '-infinity' in case of maximization)
#

#
# CP instances
#
set(instances_CP
    "instances/CP/linking.cip\;2"
    "instances/CP/j301_2.cip\;47"
    "instances/CP/disj_conj_rail.cip\;1324.43557422969"
    "instances/CP/stein27_super.cip\;1"
    )

set(settings_default
    "default"
)
#
# settings to run a whole test. In order to run a sub-suite, use -R
#
set(settings_MIP
    "default"
    "heuristics_off"
    "separating_fast"
    "presolving_fast"
    "heuristics_fast"
    "separating_aggressive"
    "presolving_aggressive"
    "heuristics_aggressive"
    "emphasis_numerics"

    #constraints
    "indicatorlogicorsepa"
    "indicatoralterlp"

    #presolving and branching
    "convertinttobin_mostinf"
    "setppclifting_leastinf"

    #branching and propagation
    "pscost_proporbitope"

    #separation and branching
    "oddcyclelift_randomhybrid"
    "oddcycle_distribution"
    "treemodel_ratio"
    "treemodel_sampling"
    "treemodel_svts_filter"
    "treemodel_svts_nofilter"

    #separation and heuristics
    "oddcycleliftheur_feaspump20"

    #heuristics and branching
    "heurlprows"
    "lns_epsgreedy_vanilla"
    "lns_exp3"
#    "dynamic"
    "intobj"
#    "nologicor"
    "xorsepaparity"
    "solvingphases"
#    "cloud"
    "uct_breadth_dualval"
    "conflictdiving"
)
#
# settings for the componentshandler test
#
set(settings_compHandlerTest
    "components_during_solve"
    )
#
# settings to run a reduced MIP test
#
set(settings_fastMIP
    "all_aggressive"
    "alldisp"
    "allfull"
    "bakvisualization"
    "fullstrongbfs"
    "cgmip"
    "cgmipstrong"
    "cgmipviol"
    "lookahead-abbrev-bincons"
    "lookahead-addclique-scaledcutoffscore"
    "presolving_off"
    "separating_off"
    "separating_misc"
    "vbcvisualization"
    "enable_treeprofile_regression_forest"
    "../../../scripts/trainEstimation/periodic_report"
  )
set(settings_MINLP
#    "emphasis_numerics"
    "minlp"
    "undercover1"
    "undercover2"
    "rensnlp"
    "nlpdiving2"
    "nlpdiving3"
    "nlpdiving1-relprop"
    "nlpdivingsolvesubmip"
    "nlpdiving4"
    "separating_aggressive"
)

set(settings_branch
    "branch_inference"
)

set(settings_Bilinrelax
    "minlp"
    "bilinrelax"
  )

set(settings_Symmetry_A
    "orbitalfixing-timing0"
    "orbitalfixing-timing1"
    "orbitalfixing-timing2"
    "orbitopes-timing0"
    "orbitopes-timing2"
    "orbitopes-timing2-static"
    )

set(settings_Symmetry_B
    "packingorbitopes-timing0"
    "packingorbitopes-timing2"
    "symresacks-timing0"
    "symresacks-timing2"
    "sst_leader-first_tiebreak-maxconflicts"
    "sst_leader-first_tiebreak-orbitmin"
    "sst_leader-maxconflicts_tiebreak-orbitmin"
    )

set(settings_Symmetry_MINLP
    "checksym"
    )

set(settings_sparsify
    "sparsifyaggr"
  )

set(settings_Benders
    "usebenders"
    "usebenders-tr"
    "usebenders-cutstrengthen"
   )

set(settings_BendersLB
    "usebenders"
   )

set(settings_BendersQP
    "benders-qp"
   )

set(settings_Indicator
    "default"
    "indicatorrestart"
    "indicatorperspective"
    "indicatorlogicorsepa"
    "indicatorcoupling"
    "indicatorconflict"
    "indicatoralterlp"
    "indicatorupgrade"
   )

set(settings_reopt
    "reopt-test-settings-usesplitcons-TRUE"
    "reopt-test-settings-varorderinterdiction-d"
    "reopt-test-settings-varorderinterdiction-i"
    "reopt-test-settings-varorderinterdiction-r"
    )


#
# Indicator instances
#
set(instances_Indicator
    "instances/Indicator/mcf128-4-1.lp\;14"
    "instances/Indicator/mcf64-4-1.lp\;10"
)

set(settings_Cardinality
    "default"
#    "upgdcardinality"
)

set(settings_Or
    "default"
    "presolving_separating_heuristics_off_randombranching"
)


#
# read/write instances
#
set(instances_MIP_write
  #"instances/MIP/bell5.mps\;8966406.49"
  #"instances/MIP/egout.mps\;568.1007"
  #"instances/MIP/flugpl.mps\;1201500"
  #"instances/MIP/MANN_a9.clq.lp\;16"
    "instances/MIP/p0033.osil\;3089"
    "instances/MIP/stein27.fzn\;18"
    "instances/MIP/stein27_inf.lp\;+infinity"
    )
#
# MIP instances
#
set(instances_MIP
    "instances/MIP/bell5.mps\;8966406.49"
    "instances/MIP/blend2.mps\;7.598985"
#    "instances/MIP/dcmulti.mps\;188182"
    "instances/MIP/egout.mps\;568.1007"
    "instances/MIP/enigma.mps\;0"
    "instances/MIP/flugpl.mps\;1201500"
    "instances/MIP/gt2.mps\;21166"
    "instances/MIP/lseu.mps\;1120"
    "instances/MIP/MANN_a9.clq.lp\;16"
    "instances/MIP/misc03.mps\;3360"
    "instances/MIP/p0033.osil\;3089"
    "instances/MIP/p0548.mps\;8691"
    "instances/MIP/rgn.mps\;82.1999974"
    "instances/MIP/stein27.fzn\;18"
    "instances/MIP/stein27_inf.lp\;+infinity"
    "instances/MIP/vpm2.fzn\;13.75"
    )
#
# Instances for component handler test
#
set(instances_compHandlerTest
    "instances/MIP/lseu_dcmulti.cip\;189302"
    "instances/MIP/4sHB.cip\;67"
    )
#
# Reduced set of MIP instances for more expensive settings
#
set(instances_fastMIP
    "instances/MIP/bell5.mps\;8966406.49"
    "instances/MIP/egout.mps\;568.1007"
    "instances/MIP/gt2.mps\;21166"
    "instances/MIP/MANN_a9.clq.lp\;16"
    "instances/MIP/p0033.osil\;3089"
    )

#
# add additional ZIMPL instances if the configuration uses ZIMPL
#
if(ZIMPL AND ZIMPL_FOUND)

list(APPEND
    instances_Indicator
    "/instances/Indicator/indicatortest.zpl\;-2"
    "instances/Indicator/mcf64-4-1.zpl\;10"
    )
#
# this instance was part of the original coverage tests. However, it causes
# a significant overhead without providing relevant benefit.
#
#list(APPEND
#    instances_MIP
#    "instances/MIP/rocII_2_11.zpl\;+4.61527698552400e+00"
#    )

endif()
#
# MINLP instances
#
set(instances_MINLP
    "instances/MINLP/bip_cross_min.10.10.10.1.pip\;1"
    "instances/MINLP/circle.lp\;4.57424778"
    "instances/MINLP/ex1266.mps\;16.3"
    "instances/MINLP/m3.osil\;37.8"
    "instances/MINLP/parincQuadratic.osil\;49920.5564"
    "instances/MINLP/tltr.mps\;48.0666666667"
    )
set(instances_branch
    "instances/MIP/egout.mps\;568.1007"
    "instances/MIP/MANN_a9.clq.lp\;16"
    "instances/MIP/p0033.osil\;3089"
    "instances/MINLP/circle.lp\;4.57424778"
    "instances/MINLP/ex1266.mps\;16.3"
    )
set(instances_Bilinrelax
    "instances/Bilinrelax/pointpack04.osil\;1.00000024988836"
    "instances/Bilinrelax/pointpack06.osil\;0.361111675167215"
  )

#
# PseudoBoolean instances
#
set(instances_PseudoBoolean
    "instances/PseudoBoolean/factor-mod-size=9-P0=67-P1=349-P2=67-P3=499-P4=79-P5=347-P6=307-B.opb\;3"
    "instances/PseudoBoolean/normalized-bsg_10_4_5.opb\;-4"
    "instances/PseudoBoolean/normalized-mds_10_4_3.opb\;2"
    "instances/PseudoBoolean/normalized-mds_50_25_5.opb\;3"
    "instances/PseudoBoolean/normalized-t2001.13queen13.1111218308.opb\;+infinity"
    "instances/PseudoBoolean/normalized-t2001.13queen13.1110976730--soft-33-100-0.wbo\;4"
    "instances/PseudoBoolean/wheel010.lap.opb.pre.cip\;25"
    )

#
# sparsify instances
#
set(instances_sparsify
    "instances/MIP/blend2.mps\;7.598985"
    "instances/PseudoBoolean/normalized-t2001.13queen13.1111218308.opb\;+infinity"
    )

#
# Semicontinuous instances
#
set(instances_Semicontinuous
#    "instances/Semicontinuous/meanvarxsc.lp\;14.36918"
    "instances/Semicontinuous/semicon1.mps\;1.1"
    )
#
# SAT instance(s)
#
set(instances_SAT
    "instances/SAT/bart10.shuffled.cnf\;0"
    )

#
# Cardinality instances. Testing with or without cardinality upgrade
# should yield the same optimal solution
#
set(instances_Cardinality
#    "instances/Cardinality/atm_5_10_1.lp\;+5.97040200941306e+04"
    "instances/Cardinality/atm_5_25_1.cip\;+1.40606905557936e+05"
    "instances/Cardinality/atm_5_25_3.cip\;+1.35160667451104e+05"
    )

#
# SOS instances
#
set(instances_SOS
    "instances/SOS/findRoot.lp\;1.0"
    "instances/SOS/pcu_0.01.lp\;0.167527525"
    "instances/SOS/pcu_0.1.lp\;0.185145653"
    "instances/SOS/sparse2.lp\;26.0"
    "instances/SOS/tpesc.lp\;54027.1672208127"
    )

#
# Orbitope/Orbisack/Symresack instances
#
set(instances_SymmetryConss
    "instances/Symmetry/packorb_1-FullIns_3.cip\;28"
    "instances/Symmetry/partorb_1-FullIns_3.cip\;4"
    "instances/Symmetry/packorbisack_1-FullIns_3.cip\;4"
    "instances/Symmetry/symresack_1-FullIns_3.cip\;28"
    )

#
# Symmetry instances
#
set(instances_Symmetry_A
    "instances/MIP/MANN_a9.clq.lp\;16"
    "instances/MIP/rgn.mps\;82.1999974"
    "instances/MIP/stein27.fzn\;18"
    )

set(instances_Symmetry_B
    "instances/MIP/stein27_inf.lp\;+infinity"
    "instances/PseudoBoolean/wheel010.lap.opb.pre.cip\;25"
    )

#
# Symmetry MINLP instances
#
set(instances_Symmetry_MINLP
    "instances/MINLP/gastrans.osil.gz\;89.0858400000"
    "instances/MINLP/tln2.osil.gz\;5.3000000000"
    "instances/MINLP/cvxnonsep_psig40r.osil.gz\;86.5450782341784"
    )

#
# Stochastic programming instances
#
set(instances_Stochastic
   "instances/Stochastic/pltexpA2_6.smps\;-9.479354404641"
   "instances/Stochastic/pltexpA2_16.smps\;-9.663308373027"
   "instances/Stochastic/pltexpA4_6.smps\;-19.5994173819041"
    "instances/Stochastic/sslp_5_25_5.smps\;-100.6"
   )

set(instances_Benders
    "instances/Stochastic/4node1.smps\;480.9"
    "instances/Stochastic/sslp_5_25_50.smps\;-121.6"
   )

set(instances_BendersLB
    "instances/Stochastic/sslp_5_25_50_LB.smps\;-121.6"
   )

set(instances_BendersQP
    "instances/Benders/classical_20_0.mps.gz\;instances/Benders/classical_20.dec.gz\;-8.22960761981176e-02"
    "instances/Benders/classical_30_0.mps.gz\;instances/Benders/classical_30.dec.gz\;-7.98150796362468e-02"
    "instances/Benders/classical_30_15.mps.gz\;instances/Benders/classical_30.dec.gz\;-7.34740783553289e-02"
   )

set(instances_Or
    "instances/Or/or_constraint.cip\;18"
    )

#
# available reader file extensions for SCIP readers
#
set(scip_reader_extensions
  "mps"
  "lp"
  "opb"
  "pip"
  "fzn"
  "cip"
  "rlp"
  )

#
# Writer output formats that are not also readers (all writers are scip_reader_extensions + scip_only_writer_extensions)
#
set(scip_writer_only_extensions
  "wbo"
  )

#
# add a test to build the SCIP binary that all further tests depend on
#
add_test(NAME scip-build
        COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target scip
        )

#
# avoid that several build jobs try to concurrently build the SCIP library
# note that this ressource lock name is not the actual libscip target
#
set_tests_properties(scip-build
                    PROPERTIES
                        RESOURCE_LOCK libscip
                    )


#
# macro to split an instance into its relevant information
# - path
# - optval
# - basename
#
macro(split_instance instance)
    list(GET instance 0 path)
    list(GET instance 1 optval)
    get_filename_component(basename ${path} NAME)
endmacro(split_instance)

macro(split_decompinstance instance)
    list(GET instance 0 instpath)
    list(GET instance 1 decpath)
    list(GET instance 2 optval)
    get_filename_component(basename ${instpath} NAME)
endmacro(split_decompinstance)

macro(add_instancetests instances settings prefix)
#
# loop over the instances
#
    foreach(instance ${${instances}})
        split_instance(instance)
        #
        # loop over all settings
        #
        foreach(setting ${${settings}})
            #
            # treat the instance as a tuple (list) of two values
            #
            add_test(NAME ${prefix}-${setting}-${basename}
                    COMMAND $<TARGET_FILE:scip> -f ${PROJECT_SOURCE_DIR}/check/${path} -s ${PROJECT_SOURCE_DIR}/check/coverage/settings/${setting}.set -o ${optval} ${optval}
                    )
            set_tests_properties(${prefix}-${setting}-${basename}
                                PROPERTIES
                                    PASS_REGULAR_EXPRESSION "Validation         : Success"
                                    FAIL_REGULAR_EXPRESSION "WARNING: unknown parameter;user parameter file <${PROJECT_SOURCE_DIR}/check/coverage/settings/${setting}.set> not found"
                                    DEPENDS scip-build
                                )
        endforeach(setting)
    endforeach(instance)
endmacro(add_instancetests)

macro(add_decompinstancetests instances settings prefix)
#
# loop over the instances
#
if(DEFINED IPOPT_FOUND AND IPOPT_FOUND)
   set(TEST_BENDERSQP TRUE)
else()
   set(TEST_BENDERSQP FALSE)
endif()
if(${TEST_BENDERSQP} AND ((NOT ${LPS} STREQUAL "msk" )))
    foreach(instance ${${instances}})
        split_decompinstance(instance)
        #
        # loop over all settings
        #
        foreach(setting ${${settings}})
            #
            # treat the instance as a tuple (list) of two values
            #
            add_test(NAME ${prefix}-${setting}-${basename}
               COMMAND $<TARGET_FILE:scip> -c "set load ${PROJECT_SOURCE_DIR}/check/coverage/settings/${setting}.set read ${PROJECT_SOURCE_DIR}/check/${instpath} read ${PROJECT_SOURCE_DIR}/check/${decpath} opt validatesol ${optval} ${optval} q"
                    )
            set_tests_properties(${prefix}-${setting}-${basename}
                                PROPERTIES
                                    PASS_REGULAR_EXPRESSION "Validation         : Success"
                                    FAIL_REGULAR_EXPRESSION "WARNING: unknown parameter"
                                    FAIL_REGULAR_EXPRESSION "user parameter file <${PROJECT_SOURCE_DIR}/check/coverage/settings/${setting}.set> not found"
                                    DEPENDS scip-build
                                )
        endforeach(setting)
    endforeach(instance)
endif()
endmacro(add_decompinstancetests)

add_instancetests(instances_CP settings_default "CP")
add_instancetests(instances_Indicator settings_Indicator "Indicator")
add_instancetests(instances_MIP settings_MIP "MIP")
add_instancetests(instances_compHandlerTest settings_compHandlerTest "compHandlerTest")
add_instancetests(instances_fastMIP settings_fastMIP "FastMIP")
add_instancetests(instances_MINLP settings_MINLP "MINLP")
add_instancetests(instances_branch settings_branch "Branch")
add_instancetests(instances_Bilinrelax settings_Bilinrelax "Bilinrelax")
add_instancetests(instances_PseudoBoolean settings_default "PseudoBoolean")
add_instancetests(instances_sparsify settings_sparsify "Sparsify")
add_instancetests(instances_Semicontinuous settings_default "Semicontinuous")
add_instancetests(instances_SAT settings_default "SAT")
add_instancetests(instances_SOS settings_default "SOS")
add_instancetests(instances_SymmetryConss settings_default "SymmetryConss")
add_instancetests(instances_Symmetry_A settings_Symmetry_A "Symmetry_A")
add_instancetests(instances_Symmetry_B settings_Symmetry_B "Symmetry_B")
add_instancetests(instances_Symmetry_MINLP settings_Symmetry_MINLP "Symmetry_MINLP")
add_instancetests(instances_Cardinality settings_Cardinality "Cardinality")
add_instancetests(instances_Stochastic settings_default "Stochastic")
add_instancetests(instances_Benders settings_Benders "Benders")
add_instancetests(instances_Or settings_Or "Or")
add_instancetests(instances_BendersLB settings_BendersLB "BendersLB")
add_decompinstancetests(instances_BendersQP settings_BendersQP "BendersQP")

#
# test writing and reading solutions
#
foreach(instance ${instances_MINLP})
    split_instance(instance)
    #
    # configure the batch file for this test by substituting placeholders in the in.file
    #
    configure_file(interactiveshell/solutiontest.bat.in interactiveshell/solutiontest-${basename}.bat)
        add_test(NAME MINLP-solutiontest-${basename}
                COMMAND $<TARGET_FILE:scip> -b ${PROJECT_BINARY_DIR}/check/interactiveshell/solutiontest-${basename}.bat
                )
        set_tests_properties(MINLP-solutiontest-${basename}
                            PROPERTIES
                                PASS_REGULAR_EXPRESSION "Validation         : Success"
                                DEPENDS scip-build
                            )
endforeach(instance)

#
# test reading .cip files for indicator instances
#
foreach(instance ${instances_Indicator})
    split_instance(instance)
    #
    # configure the batch file for this test by substituting placeholders in the in.file
    #
    configure_file(interactiveshell/solvecip.bat.in interactiveshell/solvecip-${basename}.bat)
    add_test(NAME Indicator-solvecip-${basename}
            COMMAND $<TARGET_FILE:scip> -b ${PROJECT_BINARY_DIR}/check/interactiveshell/solvecip-${basename}.bat
            )
    set_tests_properties(Indicator-solvecip-${basename} PROPERTIES PASS_REGULAR_EXPRESSION "Validation         : Success")
endforeach(instance)


#
# the following instances are not pure binary and hence cannot be written out in opb format
#
set(basenames_opb_wrongformat
    bell5.mps
    blend2.mps
    egout.mps
    flugpl.mps
    gt2.mps
    MANN_a9.clq.lp
    misc03.mps
    rgn.mps
    vpm2.fzn
    lseu_dcmulti.cip
    lseu_dcmulti_sHB.cip
    )
set(message_format_opb "WARNING: only binary problems can be written in OPB format.")

#
# the following instances use variable or equations names that are not supported in pip format
#
set(basenames_pip_wrongformat
    rgn.mps
    )
set(message_format_pip "PIP might be corrupted")

#
# test writing and reading from and to different file extensions for MIP
#
foreach(instance ${instances_MIP_write})
  foreach(extension ${scip_writer_only_extensions})
    split_instance(instance)
    configure_file(interactiveshell/writeronlytest.bat.in interactiveshell/writeronlytest-${extension}-${basename}.bat)
    set(regex "written original problem to file")

    add_test(NAME MIP-writeronlytest-${extension}-${basename}
      COMMAND $<TARGET_FILE:scip> -b ${PROJECT_BINARY_DIR}/check/interactiveshell/writeronlytest-${extension}-${basename}.bat
      )
    set_tests_properties(MIP-writeronlytest-${extension}-${basename}
      PROPERTIES
      PASS_REGULAR_EXPRESSION ${regex}
      DEPENDS scip-build
      )
  endforeach(extension)
endforeach(instance)

#
# test writing and reading from and to different file extensions for MIP
#
foreach(instance ${instances_MIP})
    split_instance(instance)
    if(${basename} STREQUAL "rgn.mps")
      continue()
    endif()

    foreach(extension ${scip_reader_extensions})
        #
        # configure the batch file for this test by substituting placeholders in the in.file
        #

        # does this instance match the requirements of this format?
        list(FIND basenames_${extension}_wrongformat ${basename} wrongformat)

        #
        # use different template batch files depending on the format requirements
        #
        if( wrongformat EQUAL -1 )
            configure_file(interactiveshell/readertest.bat.in interactiveshell/readertest-${extension}-${basename}.bat)
            set(regex "Validation         : Success.*Validation         : Success")
        else()
            configure_file(interactiveshell/readertest-wrongformat.bat.in interactiveshell/readertest-${extension}-${basename}.bat)
            set(regex "${message_format_${extension}}")
        endif()

        add_test(NAME MIP-readertest-${extension}-${basename}
                COMMAND $<TARGET_FILE:scip> -b ${PROJECT_BINARY_DIR}/check/interactiveshell/readertest-${extension}-${basename}.bat
                )
        set_tests_properties(MIP-readertest-${extension}-${basename}
                            PROPERTIES
                                PASS_REGULAR_EXPRESSION ${regex}
#                                     FAIL_REGULAR_EXPRESSION ERROR
                                DEPENDS scip-build
                            )
    endforeach(extension)
endforeach(instance)

#
# add tests for the tpi
#
if(NOT ${TPI} STREQUAL "none")
    foreach(instance ${instances_MIP})
    split_instance(instance)
        #
        # configure the batch file for this test by substituting placeholders in the in.file
        #
        configure_file(interactiveshell/concurrentsolve.bat.in interactiveshell/concurrentsolve-${basename}.bat)
        add_test(NAME MIP-concurrentsolve-${basename}
                COMMAND $<TARGET_FILE:scip> -b ${PROJECT_BINARY_DIR}/check/interactiveshell/concurrentsolve-${basename}.bat
                )
        set_tests_properties(MIP-concurrentsolve-${basename}
                            PROPERTIES
                                PASS_REGULAR_EXPRESSION "Validation         : Success"
                                DEPENDS scip-build
                            )
    endforeach(instance)
endif()

#
# add tests that read partial solutions
#
foreach(instance ${instances_MIP})
    split_instance(instance)
    #
    # if there is a partial solution file in mipstarts, we add a test
    #
    if( EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/mipstarts/${basename}.mst" )
        #
        # configure the template batch file and store it under the binary (build) directory
        #
        configure_file(interactiveshell/mipstarts.bat.in interactiveshell/MIP-mipstart-${basename}.bat)

        #
        # add test that executes the configured batch file and greps the validation expression
        #
        add_test(NAME MIP-mipstart-${basename}
                COMMAND $<TARGET_FILE:scip> -b interactiveshell/MIP-mipstart-${basename}.bat
                )
        set_tests_properties(MIP-mipstart-${basename}
                            PROPERTIES
                                PASS_REGULAR_EXPRESSION "Validation         : Success"
                                # the MIP start instances are validated twice.
                                FAIL_REGULAR_EXPRESSION "Validation         : Fail"
                                DEPENDS scip-build
                            )
    endif()
endforeach(instance)

#
# interactive shell test.
# We substitute first path placeholders in the .in file, which creates a new file under the binary directory
# Then run SCIP to execute the list of commands defined there.
#
set(shell_tmp_dir ${PROJECT_BINARY_DIR}/check/temp)
file(MAKE_DIRECTORY ${shell_tmp_dir})

configure_file(interactiveshell/interactiveshell.bat.in interactiveshell/interactiveshell.bat)

add_test(NAME interactiveshell
        COMMAND $<TARGET_FILE:scip> -b ${PROJECT_BINARY_DIR}/check/interactiveshell/interactiveshell.bat
        )
set_tests_properties(interactiveshell
                    PROPERTIES
                        PASS_REGULAR_EXPRESSION "increased branching priority of new binary variables"
                        DEPENDS scip-build
                    )

add_test(NAME write_and_read_settings
        COMMAND $<TARGET_FILE:scip> -b ${CMAKE_CURRENT_SOURCE_DIR}/interactiveshell/write_and_read_settings.bat
        )
set_tests_properties(write_and_read_settings
                    PROPERTIES
                        FAIL_REGULAR_EXPRESSION "ERROR"
                        DEPENDS scip-build
                    )

#
# reopt test.
# We substitute first path placeholders in the .in file, which creates a new file under the binary directory
# Then run SCIP to execute the list of commands defined there.
#

set(shell_tmp_dir ${PROJECT_BINARY_DIR}/check/temp)
file(MAKE_DIRECTORY ${shell_tmp_dir})

# disabled reopt on FullIns_3 until #2758 is dealt with
# foreach(testname reopt-chg-obj-stein27 reopt-chg-obj-flugpl reopt-chg-obj-1-FullIns_3)
foreach(testname reopt-chg-obj-stein27 reopt-chg-obj-flugpl)
    foreach(setting ${settings_reopt})
        configure_file(interactiveshell/${testname}.bat.in interactiveshell/${testname}-${setting}.bat)
        add_test(NAME ${testname}-${setting}
                COMMAND $<TARGET_FILE:scip> -b ${PROJECT_BINARY_DIR}/check/interactiveshell/${testname}-${setting}.bat
                )
        set_tests_properties(${testname}-${setting} PROPERTIES
                                            PASS_REGULAR_EXPRESSION "Validation         : Success"
                                            DEPENDS scip-build)
        endforeach()

endforeach()


#
# define sets of linear classification results
#

set(linclass_bell5
  "total             :         91"
  "singleton         :          2"
  "precedence        :         17"
  "varbound          :         18"
  "mixedbinary       :         26"
  "general           :         28"
  )

set(linclass_blend2
    "total             :        274"
    "singleton         :         88"
    "aggregation       :          9"
    "varbound          :         88"
    "intknapsack       :          0"
    "mixedbinary       :         68"
    "general           :         21"
    )
set(linclass_dcmulti
    "setpacking        :          5"
    "cardinality       :          3"
    "invknapsack       :         10"
    )

set(linclass_egout
    "total             :         98"
    "aggregation       :         10"
    "varbound          :         55"
    "knapsack          :          0"
    "intknapsack       :          0"
    "mixedbinary       :         33"
    "general           :          0"
    )

set(linclass_enigma
    "total             :         21"
    "varbound          :          0"
    "setpartition      :         20"
    "setpacking        :          0"
    "eqknapsack        :          1"
    )
set(linclass_flugpl
    "total             :         18"
    "empty             :          0"
    "free              :          0"
    "singleton         :          1"
    "varbound          :          6"
    "mixedbinary       :          0"
    "general           :         11"
    )
set(linclass_gt2
    "intknapsack       :         15"
    )
set(linclass_lseu
    "knapsack          :         11"
    )
set(linclass_misc03
    "setpartition      :          5"
    "setpacking        :          3"
    "setcovering       :         31"
    "cardinality       :         21"
    "invknapsack       :         33"
    "eqknapsack        :          0"
    "binpacking        :          2"
    )
set(linclass_p0548
    "empty             :          0"
    "free              :          0"
    "singleton         :          2"
    "aggregation       :          0"
    "precedence        :         12"
    "varbound          :         28"
    "setpartition      :          0"
    "setpacking        :         22"
    "setcovering       :          0"
    "cardinality       :          0"
    "invknapsack       :          0"
    "eqknapsack        :          0"
    "binpacking        :          5"
    "knapsack          :        107"
    )

set(linclasstests
    linclass_bell5
    linclass_blend2
    linclass_dcmulti
    linclass_egout
    linclass_enigma
    linclass_flugpl
    linclass_gt2
    linclass_lseu
    linclass_misc03
    linclass_p0548
    )

foreach(instance ${instances_MIP})
    split_instance(${instance})
    get_filename_component(shortbasename ${basename} NAME_WE)
    LIST(FIND linclasstests linclass_${shortbasename} linclassfound)
    if(NOT linclassfound EQUAL -1 )
        configure_file(interactiveshell/linclass.bat.in interactiveshell/linclass-${basename}.bat)
        add_test(NAME MIP-linclass-${basename}
                COMMAND $<TARGET_FILE:scip> -b interactiveshell/linclass-${basename}.bat
                )
        set_tests_properties(MIP-linclass-${basename}
                            PROPERTIES
                                DEPENDS scip-build
                            )
        foreach( pass_regex ${linclass_${shortbasename}} )
        set_tests_properties(MIP-linclass-${basename}
                            PROPERTIES
                                PASS_REGULAR_EXPRESSION "${pass_regex}"
                            )
        endforeach()

    endif()

endforeach()

#
# add a test for handling of the SIGTERM signal. The test uses the timeout command that
# is only available on Linux, that is available on MAC OS as "gtimeout" after installing
# the 'coreutils' package
#
if (TEST_SIGNAL_HANDLING)
if (UNIX)
    if (APPLE)
        set(timeoutcommand "gtimeout")
    else ()
        set(timeoutcommand "timeout")
    endif (APPLE)
    configure_file(interactiveshell/signal-handling-sigterm.bat.in interactiveshell/signal-handling-sigterm.bat)
    add_test(NAME signal-handling-sigterm
            COMMAND ${timeoutcommand} -sSIGTERM 1 $<TARGET_FILE:scip> -b interactiveshell/signal-handling-sigterm.bat
            )
    set_tests_properties(signal-handling-sigterm
                        PROPERTIES
                            DEPENDS scip-build
                            PASS_REGULAR_EXPRESSION "termination signal received"
                            #
                            # I assume that this test takes longer than 2 seconds
                            #
                            )
endif (UNIX)
endif()
