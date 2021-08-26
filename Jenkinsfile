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

node('coverity') {

    stage("SCM Checkout") {
       git url: 'https://bitbucket.microchip.com/scm/unge/sw-arm-trusted-firmware.git'
    }

    stage("Compile and analysis") {
        catchError {
            sh "rm -fr build"
            sh "ruby ./scripts/build.rb --platform lan966x_sr"
            sh "ruby ./scripts/build.rb --platform lan966x_evb"
        }
    }
}
