reg = /i1\s+([0-9].[0-9]+)/

str = File.read("data_big_x.txt")


arr = Hash.new
str.scan(reg) {|m| 
  arr[m[0]] = true
}

puts arr.length

