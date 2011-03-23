#!/usr/bin/env ruby

require File.join(File.dirname(File.dirname(File.expand_path(__FILE__))),
                  'external/dist/share/service_testing/bp_service_runner.rb')
require 'uri'
require 'test/unit'
require 'open-uri'
require 'rbconfig'
include Config

# NEEDSWORK!!  DISABLED UNTIL WE FIGURE OUT HOW TO VALIDATE WITHOUT GEMS
# Neil (ndmanvar) wrote this test using gems:
# rubyzip gem
# tarruby gem
#require 'rubygems'
#require 'zip/zip'
##require 'zip/ZipFileSystem'
##require 'archive/tar/minitar'
#require 'tarruby'

class TestArchiver < Test::Unit::TestCase
  def setup
    subdir = 'build/Archiver'
    if ENV.key?('BP_OUTPUT_DIR')
      subdir = ENV['BP_OUTPUT_DIR']
    end
    @cwd = File.dirname(File.expand_path(__FILE__))
    @service = File.join(@cwd, "../#{subdir}")
    @providerDir = File.expand_path(File.join(@cwd, "providerDir"))
    @path1 = @cwd + "/test_files/"
    @path_testdir = @path1 + "test_directory/"
    @path_testdir_noP = @path1 + "test_directory"
    @path_testdir1 = @path1 + "test_directory/test_directory_1/"
    @test_directory = "path://" + @path1 + "test_directory/"
    @test_directory_1 = "path://" + @path1 + "test_directory/test_directory_1"
  end
  
  def teardown
  end

  def test_load_service
    BrowserPlus.run(@service, @providerDir) { |s|
    }
  end

  # BrowserPlus.Archiver.archive({params}, function{}())
  # Lets you archive and optionally compress files and directories.
  # (avaliable formats are zip, zip (uncompressed), tar, tar.gx, and tar.bz2)
  def test_zip_one_file
    BrowserPlus.run(@service, @providerDir) { |s|
      # One directory - zip.
assert_equal(0, 1)
## NEEDSWORK!!  FAILS ON CODE COVERAGE!!
#      output = s.archive({ 'files' => [@test_directory_1], 'format' => 'zip', 'recurse' => false })
#      # Open zip, compare files name/contents to original.
#      Zip::ZipFile.open(output['archiveFile']) { |zipfile|
#        want = File.open(@path_testdir1 + "/bar1.txt", "rb") { |f| f.read }
#        got = zipfile.read("test_directory_1/bar1.txt")
#        assert_equal(want, got)
#        want = File.open(@path_testdir1 + "/bar2.txt", "rb") { |f| f.read }
#        got = zipfile.read("test_directory_1/bar2.txt")
#        assert_equal(want, got)
#        want = File.open(@path_testdir1 + "/bar3.txt", "rb") { |f| f.read }
#        got = zipfile.read("test_directory_1/bar3.txt")
#        assert_equal(want, got)
#      }
#      File.delete(output['archiveFile'])
    }
  end

  # BrowserPlus.Archiver.archive({params}, function{}())
  # Lets you archive and optionally compress files and directories.
  # (avaliable formats are zip, zip (uncompressed), tar, tar.gx, and tar.bz2)
  def test_zip_two_file
    BrowserPlus.run(@service, @providerDir) { |s|
      # Two files - zip.
## NEEDSWORK!!  FAILS ON CODE COVERAGE!!
#      output = s.archive({ 'files' => [@test_directory_1 + "/bar1.txt", @test_directory_1 + "/bar2.txt"], 'format' => 'zip', 'recurse' => false })
#      # Open zip, compare files name/contents to original.
#      Zip::ZipFile.open(output['archiveFile']) { |zipfile|
#        want = File.open(@path_testdir1 + "/bar1.txt", "rb") { |f| f.read }
#        got = zipfile.read("bar1.txt")
#        assert_equal(want, got)
#        want = File.open(@path_testdir1 + "/bar2.txt", "rb") { |f| f.read }
#        got = zipfile.read("bar2.txt")
#        assert_equal(want, got)
#      }
#      File.delete(output['archiveFile'])
#      # Still need to add test cases that test: followLinks, recurse, progressCallback.
##      output = s.archive({ 'files' => [@test_directory], 'format' => 'zip', 'recurse' => true })
##      Zip::ZipFile.open(output['archiveFile']) { |zipfile|
##        q = zipfile.read(@test_directory_1 + "/bar1.txt")
##      }
##      File.delete(output['archiveFile'])
    }
  end

  # Still working on tar.
  # BrowserPlus.Archiver.archive({params}, function{}())
  # Lets you archive and optionally compress files and directories.
  # (avaliable formats are zip, zip (uncompressed), tar, tar.gx, and tar.bz2)
  def test_tar
    BrowserPlus.run(@service, @providerDir) { |s|
## NEEDSWORK!!  FAILS ON CODE COVERAGE!!
#      output = s.archive({ 'files' => [@test_directory_1], 'format' => 'tar' , 'recurse' => false })
#      Tar.open(output['archiveFile'], File::RDONLY, 0644, Tar::GNU | Tar::VERBOSE) do |tar|
#        # Or 'tar.each do ...'
#        while tar.read
#          # Regular file.
#          if tar.reg? && tar.pathname != "test_directory_1/.DS_Store"
#            tar.extract_file('test')
#            want = File.read(File.join(@path_testdir, tar.pathname))
#            got = File.read('test')
#            # Asserting bar1,2,3 from tar file is same as original bar1,2,3.
#            assert_equal(want, got)
#          end
#        end
#        # If extract all files.
#        #tar.extract_all
#      end
#      # For gzip archive.
#      #Tar.gzopen('foo.tar.gz', ...
#      # For bzip2 archive.
#      #Tar.bzopen('foo.tar.bz2', ...
#      File.delete(output['archiveFile'])
    }
  end

  # BrowserPlus.Archiver.archive({params}, function{}())
  # Lets you archive and optionally compress files and directories.
  # (avaliable formats are zip, zip (uncompressed), tar, tar.gx, and tar.bz2)
  def test_gzip
    BrowserPlus.run(@service, @providerDir) { |s|
## NEEDSWORK!!  FAILS ON WINDOWS!!
#      output = s.archive({ 'files' => [@test_directory_1], 'format' => 'tar-bzip2' , 'recurse' => false })
#      #Tar.bzopen(@output['archiveFile'], File::RDONLY, 0644, Tar::GNU | Tar::VERBOSE) do |tar|
#      #end
#      File.delete(output['archiveFile'])
    }
  end
end
