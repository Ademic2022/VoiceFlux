#!/bin/sh
#
# Gradle start up script for UN*X
#

# Attempt to set APP_HOME
PRG="$0"
while [ -h "$PRG" ] ; do
    ls=$(ls -ld "$PRG")
    link=$(expr "$ls" : '.*-> \(.*\)$')
    if expr "$link" : '/.*' > /dev/null; then
        PRG="$link"
    else
        PRG=$(dirname "$PRG")"/$link"
    fi
done
SAVED="$(pwd)"
cd "$(dirname "$PRG")/" || exit
APP_HOME="$(pwd -P)"
cd "$SAVED" || exit

APP_NAME="Gradle"
APP_BASE_NAME=$(basename "$0")

# Add default JVM options here.
DEFAULT_JVM_OPTS="-Xmx64m -Xms64m"

# Find java
if [ -n "$JAVA_HOME" ] ; then
    if [ -x "$JAVA_HOME/jre/sh/java" ] ; then
        JAVACMD="$JAVA_HOME/jre/sh/java"
    else
        JAVACMD="$JAVA_HOME/bin/java"
    fi
    if [ ! -x "$JAVACMD" ] ; then
        die "ERROR: JAVA_HOME is set to an invalid directory: $JAVA_HOME"
    fi
else
    JAVACMD="java"
    which java > /dev/null 2>&1 || die "ERROR: JAVA_HOME is not set and no 'java' command could be found."
fi

CLASSPATH=$APP_HOME/gradle/wrapper/gradle-wrapper.jar

exec "$JAVACMD" \
  $DEFAULT_JVM_OPTS \
  -classpath "$CLASSPATH" \
  org.gradle.wrapper.GradleWrapperMain \
  "$@"
