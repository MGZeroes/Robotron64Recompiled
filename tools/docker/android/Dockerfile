# Android test build image (ADR 007): SDK, NDK and Gradle on top of ubuntu:24.04. Separate from
# tools/docker/Dockerfile so the desktop and decomp image stays small. Versions follow what the Goemon
# Android port was validated with (NDK 27.1, AGP 8.1, Gradle 8.0, JDK 17).
FROM ubuntu:24.04

ARG DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
        openjdk-17-jdk-headless \
        cmake ninja-build clang lld python3 \
        git rsync unzip curl ca-certificates file \
    && rm -rf /var/lib/apt/lists/*

ENV ANDROID_HOME=/opt/android-sdk
ARG CMDLINE_TOOLS=11076708
RUN mkdir -p $ANDROID_HOME/cmdline-tools \
    && curl -fsSL -o /tmp/tools.zip https://dl.google.com/android/repository/commandlinetools-linux-${CMDLINE_TOOLS}_latest.zip \
    && unzip -q /tmp/tools.zip -d $ANDROID_HOME/cmdline-tools \
    && mv $ANDROID_HOME/cmdline-tools/cmdline-tools $ANDROID_HOME/cmdline-tools/latest \
    && rm /tmp/tools.zip \
    && yes | $ANDROID_HOME/cmdline-tools/latest/bin/sdkmanager --licenses > /dev/null \
    && $ANDROID_HOME/cmdline-tools/latest/bin/sdkmanager --install \
        "platforms;android-34" "build-tools;34.0.0" "platform-tools" "ndk;27.1.12297006" > /dev/null

ENV ANDROID_NDK=/opt/android-sdk/ndk/27.1.12297006

ARG GRADLE_VERSION=8.0
ARG GRADLE_SHA256=4159b938ec734a8388ce03f52aa8f3c7ed0d31f5438622545de4f83a89b79788
RUN curl -fsSL -o /tmp/gradle.zip https://services.gradle.org/distributions/gradle-${GRADLE_VERSION}-bin.zip \
    && echo "${GRADLE_SHA256}  /tmp/gradle.zip" | sha256sum -c - \
    && unzip -q /tmp/gradle.zip -d /opt && rm /tmp/gradle.zip \
    && ln -s /opt/gradle-${GRADLE_VERSION}/bin/gradle /usr/local/bin/gradle

WORKDIR /work
