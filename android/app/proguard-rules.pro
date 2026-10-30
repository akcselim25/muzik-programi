# Aurns Proguard kuralları: Maksimum APK küçültme ve sıfır hantallık
-dontwarn okhttp3.**
-dontwarn okio.**
-keepattributes *Annotation*
-keepclassmembers class * {
    @org.json.** <methods>;
}
