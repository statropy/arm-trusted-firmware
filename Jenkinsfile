/*
 Microchip is aware that some terminology used in this technical document is
 antiquated and inappropriate. As a result of the complex nature of software
 where seemingly simple changes have unpredictable, and often far-reaching
 negative results on the software's functionality (requiring extensive retesting
 and revalidation) we are unable to make the desired changes in all legacy
 systems without compromising our product or our clients' products.
*/

properties([
        [$class: 'BuildDiscarderProperty', strategy: [$class: 'LogRotator', artifactDaysToKeepStr: '', artifactNumToKeepStr: '', daysToKeepStr: '5', numToKeepStr: '20']],
        ])

node('blademaster') {

    stage("SCM Checkout") {
        checkout([
            $class: 'GitSCM',
            branches: scm.branches,
            doGenerateSubmoduleConfigurations: scm.doGenerateSubmoduleConfigurations,
            submoduleCfg: [],
            userRemoteConfigs: scm.userRemoteConfigs
        ])
    }

    try {
        stage("Build") {
            sh "ruby ./scripts/platform_build.rb"
        }
    } catch (error) {
        throw error
    } finally {
        echo "Branch is: ${env.BRANCH_NAME}"
        if (env.BRANCH_NAME =~ /\.b0/) {
            stage("Archiving Boot ROM results") {
                    archive 'artifacts/lan966x_b0-*.bl1'
                    archive 'artifacts/lan966x_sr-*.bl1'
                    archive 'artifacts/*b0-release-bl2normal-auth-bl1.dump'
                    archive 'artifacts/*sr-*-bl2normal-auth-bl1.dump'
            }
        } else {
            stage("Archiving FIP results") {
                    archive 'artifacts/**'
                    archive 'keys/*'
            }
        }
    }
}
