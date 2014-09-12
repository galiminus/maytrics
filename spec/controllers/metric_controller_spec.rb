require 'spec_helper'

describe "MetricController" do
  before do
    @pid = Process.spawn "bin/maytrics"
    sleep 2
  end

  after do
    Process.kill 'TERM', @pid
  end

  it "save a metric" do
    response = RestClient.post "#{$host}/testuser/testmetric.json", {value: 3}.to_json
    response.code.should == 201

    response = RestClient.get "#{$host}/testuser/testmetric.json"
    JSON.parse(response).should == {"metric" => "testmetric", "value" => 3}
  end

  it "should fail if metric is not json" do
    RestClient.post "#{$host}/testuser/testmetric.json", {value: 3} do |response|
      response.code.should == 400
    end
  end

  it "should fail if metric is not valid" do
    RestClient.post "#{$host}/testuser/testmetric.json", {value: -1}.to_json do |response|
      response.code.should == 400
    end

    RestClient.post "#{$host}/testuser/testmetric.json", {value: 11}.to_json do |response|
      response.code.should == 400
    end
  end

  it "should return 404 if metric is not found" do
    RestClient.get "#{$host}/testuser/notestmetric.json" do |response|
      response.code.should == 404
    end
  end

  it "should return 404 if metrics are not found" do
    RestClient.get "#{$host}/notestuser.json" do |response|
      response.code.should == 404
    end
  end

  it "should save several metrics" do
    response = RestClient.post "#{$host}/testuser/testmetric2.json", {value: 5}.to_json
    response.code.should == 201

    response = RestClient.get "#{$host}/testuser.json"
    JSON.parse(response).should == {"testmetric" => 3, "testmetric2" => 5}
  end
end

