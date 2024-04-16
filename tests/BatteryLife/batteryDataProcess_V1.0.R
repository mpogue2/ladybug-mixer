# Load the package required to read JSON files.
#install.packages("rjson") # Optional
library("rjson")
library("tidyverse")
library("lubridate")
library("glue")
library("slider")

# **** HOW TO FETCH DATA FROM PUSHDATA.IO *****
# wget https://pushdata.io/mpogue@zenstarstudio.com/battery_VDC

op <- options()
options(digits = 8, pillar.sigfig = 8)

rm(list=ls())
theme_set(theme_bw())

# load it
setwd("/Users/mpogue/____MIXER_PROJECT/TEST/Battery tests")

# UTILITY FUNCTIONS ----------
between <- function(x,a,b) {
  return(x >= a & x <= b)
}

# --------------------
getOneFile <- function(filename) {
  a <- fromJSON(file=filename)
  b <- as.data.frame(t(as.data.frame(a$points))) # every other row is time, and value
  head(b)
  
  times  <- as.data.frame(slice(b, seq(1, nrow(b), 2)) %>% select(datetime = V1))
  values <- as.data.frame(slice(b, seq(2, nrow(b), 2)) %>% select(value = V1))
  
  data <- tibble(datetime = times$datetime, value = values$value) %>% 
    mutate(datetime = as_datetime(datetime),
           value = as.numeric(value))
  
  data
}

USBrechargeableSTART <- ymd_hms("2024-03-06 19:00:00")
USBrechargeableEND   <- ymd_hms("2024-03-10 20:00:00")

AMZNalkalineSTART <- ymd_hms("2024-03-11 00:00:00")
AMZNalkalineEND   <- ymd_hms("2024-03-18 15:00:00")

TENERGYlithiumSTART <- ymd_hms("2024-03-31 22:00:00")
TENERGYlithiumEND <- ymd_hms("2024-04-20 00:00:00")

# keep just the data we want for the USB rechargeable runand alkaline runs
data1 <- getOneFile("battery_VDC.good")

USBrechargeable <- getOneFile("battery_VDC.good") %>% 
  filter(between(datetime, USBrechargeableSTART, USBrechargeableEND))   # keep only real data points

AMZNalkaline <- getOneFile("battery_VDC.good") %>% 
  filter(between(datetime, AMZNalkalineSTART, AMZNalkalineEND))   # keep only real data points

TENCENTlithium <- getOneFile("battery_VDC") %>% 
  filter(between(datetime, TENERGYlithiumSTART, TENERGYlithiumEND))   # keep only real data points

# ===========================
# series <- USBrechargeable
# name <- "USB rechargeable"
processOneBattery <- function(series, name) {
  batteryDeadLevel_volts <- 5.5 # below this, and we're dead, because diode drop and LDO drops out
  
  seriesAlive <- series %>% filter(value >= batteryDeadLevel_volts) # just the "mixer is alive" points
  
  batteryDuration <- difftime(last(seriesAlive)$datetime, first(seriesAlive$datetime), units="hours")
  print(glue("'{name}' usable lifetime: {round(batteryDuration, digits=1)} hours ~= {round(4.0 * batteryDuration, digits = 0)}mAh"))
}

# BATTERY LIFETIMES ===========================
processOneBattery(USBrechargeable, "USB rechargeable")
processOneBattery(AMZNalkaline, "Amazon Basics alkaline")
processOneBattery(TENCENTlithium, "Tencent lithium")

# batteryDuration <- last(USBrechargeable)$datetime - first(USBrechargeable$datetime)
# difftime(last(USBrechargeable)$datetime, first(USBrechargeable$datetime), units="hours")

# PLOTS ============================
lookBackward <- 15
goodUSB <- USBrechargeable %>% 
  filter(value > 5.5) %>% 
  mutate(type="USB rechargeable ($7.50)", 
         zDatetime = datetime - first(datetime),
         fValue = slide_dbl(value, min, .before = lookBackward)) # fValue is a filtered version of value

# dropped to zero, so add a sample at the threshold 5.5v...
# 2024-03-10 19:54:52 9.198088 USB rechargeable ($7.50) 346298.64 secs 9.198088
# tail(goodUSB)
lastSample <- tibble(datetime = (goodUSB %>% tail(1))$datetime + duration(2, units = "minutes"), 
                    value = 5.5,
                    type = "USB rechargeable ($7.50)", 
                    zDatetime = (goodUSB %>% tail(1))$zDatetime + 60.0, 
                    fValue = 5.5)
goodUSB <- rbind(goodUSB, lastSample)
tail(goodUSB)
lastUSB <- last(goodUSB)$zDatetime

goodAlkaline <- AMZNalkaline %>% 
  filter(value > 5.5) %>% 
  mutate(type="Amazon Basics alkaline ($1.50)", 
         zDatetime = datetime - first(datetime),
         fValue = slide_dbl(value, min, .before = lookBackward))
lastAlkaline <- last(goodAlkaline)$zDatetime

goodLithium <- TENCENTlithium %>% 
  filter(value > 5.5) %>% 
  mutate(type="Tencent Lithium ($7.00)", 
         zDatetime = datetime - first(datetime),
         fValue = slide_dbl(value, min, .before = lookBackward))
lastLithium <- last(goodLithium)$zDatetime

allBatteries <- rbind(goodUSB, goodAlkaline, goodLithium)
labelSize <- 4.0


p1 <- 
  ggplot(allBatteries, aes(x=zDatetime/3600, y = fValue, color = type)) +
  geom_line() +
  geom_hline(yintercept = 5.5, color = "red", linetype = "dashed") +
  geom_vline(xintercept = lastUSB/3600.0, color = "blue", linetype = "dotted") +
  geom_vline(xintercept = lastAlkaline/3600.0, color = "red", linetype = "dotted") +
  geom_vline(xintercept = lastLithium/3600.0, color = "darkgreen", linetype = "dotted") +
  annotate("label", x = lastUSB/3600.0, y = 10.0, label = glue("{round(lastUSB/3600, digits=1)} hr"), size=labelSize, color = "blue") +
  annotate("label", x = lastAlkaline/3600.0, y = 10.0, label = glue("{round(lastAlkaline/3600, digits=1)} hr"), size=labelSize, color = "red") +
  annotate("label", x = lastLithium/3600.0, y = 10.0, label = glue("{round(lastLithium/3600, digits=1)} hr"), size=labelSize, color = "darkgreen") +
  xlab("battery life (hours)") +
  ylab("battery voltage") +
  scale_x_continuous(limits = c(0, 325), breaks = seq(0, 500, 25)) +
  scale_y_continuous(limits = c(0, 10.0), breaks = seq(0, 10, 1.0)) +
  geom_hline(yintercept = 7.2, color = "darkgreen", linetype = "dotted") +
  geom_hline(yintercept = 6.0, color = "red", linetype = "dotted") +
  ggtitle("Battery Performance in Ladybug Mixer V2.1")

png("batteryLife_3.png", width = 640, height = 480)
  print(p1)
dev.off()

# ================================
# USB rechargeable lasted 96.2 hours!  That's about 32 dances.  DISADV: Voltage went to zero in 2 minutes,
#  with absolutely no warning, no downward slope at all.
# Amazon Basics Alkaline test starts 5:30PM Sunday, March 10

# ggplot(USBrechargeable, aes(x=datetime, y=value)) +
#   geom_point()
