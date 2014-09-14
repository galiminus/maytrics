require 'spec_helper'

describe "UserController" do
  before do
    system "redis-cli flushall"
    @pid = Process.spawn "bin/maytrics"
    sleep 2
  end

  after do
    Process.kill 'TERM', @pid
  end

  it "update a user" do
    response = RestClient.put "#{$host}/testuser.json", {username: "testusername"}.to_json
    response.code.should == 200

    response = RestClient.get "#{$host}/testuser.json"
    JSON.parse(response).should == {"username" => "testusername"}
  end

  it "should fail if user is not json" do
    RestClient.put "#{$host}/testuser.json", {username: "testusername"} do |response|
      response.code.should == 400
    end
  end

  it "should fail if user is not valid" do
    RestClient.put "#{$host}/testuser.json", {}.to_json do |response|
      response.code.should == 400
    end

    RestClient.put "#{$host}/testuser.json", {username: ""}.to_json do |response|
      response.code.should == 400
    end
  end

  it "should return 404 if user is not found" do
    RestClient.get "#{$host}/nouser.json" do |response|
      response.code.should == 404
    end
  end

  it "should can update" do
    response = RestClient.put "#{$host}/testuser.json", {username: "testusername"}.to_json
    response.code.should == 200
    response = RestClient.put "#{$host}/testuser.json", {username: "testusername2"}.to_json
    response.code.should == 200


    response = RestClient.get "#{$host}/testuser.json"
    JSON.parse(response).should == {"username" => "testusername2"}
  end
end

